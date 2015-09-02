/**
 * Helvetica で英文を表示するプログラム。単語単位でラップする。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "util.h"
// time関数の為に time.h をインクルードする。
#include <time.h>
#include <ctype.h>

#define FONT_HELVETICA_14 "-adobe-helvetica-medium-r-*-*-14-*-*-*-*-*-iso8859-1"
// #define FONT_HELVETICA_14 "-*-helvetica-medium-r-*-*-14-*-*-*-*-*-koi8-*"
// #define FONT_HELVETICA_14 "-cronyx-helvetica-medium-r-*-*-14-*-*-*-*-*-iso8859-5"


int NextToken(const char *str, size_t start, size_t *end)
{
    size_t index = start;

    /* トークナイズするものがない */
    if (str[index] == '\0')
	return 0;

    if (isspace(str[index])) {
	index++;
    } else {
	index++;
	while (str[index] && !isspace(str[index]))
	    index++;
    }
    *end = index;
    return 1;
}

int WordWidth(XFontStruct *font, const char *str, int len)
{
    int i;
    int width = 0;

    for (i = 0; i < len; i++) {
	XCharStruct *info = GetCharInfo(font, str[i]);

	width += info->width;
    }
    return width;
}

void Redraw(Display *disp, Window win, GC gc, XFontStruct *font, const char *msg)
{
	const int LEFT_MARGIN = 50;
	const int LINE_HEIGHT = 20;
	const int RIGHT_MARGIN = 500;
	const int TOP_MARGIN = 50;

	XClearWindow(disp, win);
	size_t start = 0, next;
	int x = LEFT_MARGIN; // left margin
	int y = TOP_MARGIN + font->ascent;
	
	while (NextToken(msg, start, &next)) {
	    size_t len = next - start;
	    int width = WordWidth(font, msg + start, len);

	    if (x + width > RIGHT_MARGIN) {
		x = LEFT_MARGIN;
		y += LINE_HEIGHT;

		if (isspace(msg[start])) // 空白位置で改行する場合は描画しない。
		    goto nextIter;
	    }
	    XDrawString(disp, win, gc,
			x,	// X座標
			y,	// Y座標。ベースライン
			msg + start,
			len);
	    x += width;

	nextIter:
	    start = next;
	}
}

void Initialize(Display **pdisp, Window *pwin, GC *pgc, XFontStruct **pfont)
{

    *pdisp = XOpenDisplay(NULL); // open $DISPLAY
    *pwin = XCreateSimpleWindow(*pdisp,						// ディスプレイ
				DefaultRootWindow(*pdisp),			// 親ウィンドウ
				0, 0,						// (x, y)
				640, 480,					// 幅・高さ
				0,						// border width
				0,						// border color
				WhitePixel(*pdisp, DefaultScreen(*pdisp)));	// background color
    XMapWindow(*pdisp, *pwin);

    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    *pgc = XCreateGC(*pdisp, *pwin, 0, NULL);
    XSetForeground(*pdisp, *pgc,
		   BlackPixel(*pdisp, DefaultScreen(*pdisp)));

    // 暴露イベントを受け取る。
    XSelectInput(*pdisp, *pwin, ExposureMask);

    *pfont = XLoadQueryFont(*pdisp, FONT_HELVETICA_14);
    XSetFont(*pdisp, *pgc, (*pfont)->fid);
}

void CleanUp(Display *disp, Window win, GC gc, XFontStruct *font)
{
    XUnloadFont(disp, font->fid);
    XFreeGC(disp, gc);
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}

int main()
{
    Display* disp;
    Window win;
    GC gc;
    XFontStruct *font;

    Initialize(&disp, &win, &gc, &font);

    XEvent ev;

    const char *msg = "Lorem ipsum dolor sit amet, "
	"consectetur adipiscing elit, sed do eiusmod "
	"tempor incididunt ut labore et dolore magna aliqua. "
	"Ut enim ad minim veniam, quis nostrud exercitation "
	"ullamco laboris nisi ut aliquip ex ea commodo consequat. "
	"Duis aute irure dolor in reprehenderit in voluptate "
	"velit esse cillum dolore eu fugiat nulla pariatur. "
	"Excepteur sint occaecat cupidatat non proident, sunt "
	"in culpa qui officia deserunt mollit anim id est laborum.";

    const char *harry = "Mr and Mrs Dursley, of number four, Privet Driver, were proud to say that they were perfectly normal, thank you very much. They were the last people you'd expect to be involved in anything strange or mysterious, because they just didn't hold with such nonsense.";

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	if (ev.type != Expose)
	    continue;

	Redraw(disp, win, gc, font, msg);
    }

    CleanUp(disp, win, gc, font);
}
