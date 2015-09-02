/**
 * Helvetica で英文を表示するプログラム。文字単位でラップする。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "util.h"
// time関数の為に time.h をインクルードする。
#include <time.h>

#define FONT_HELVETICA_14 "-adobe-helvetica-medium-r-*-*-14-*-*-*-*-*-iso8859-1"
// #define FONT_HELVETICA_14 "-*-helvetica-medium-r-*-*-14-*-*-*-*-*-koi8-*"
// #define FONT_HELVETICA_14 "-cronyx-helvetica-medium-r-*-*-14-*-*-*-*-*-iso8859-5"

int main()
{
    Display* disp;

    disp = XOpenDisplay(NULL); // open $DISPLAY

    Window win;
    win = XCreateSimpleWindow(disp,					// ディスプレイ
			      DefaultRootWindow(disp),			// 親ウィンドウ
			      0, 0,					// (x, y)
			      640, 480,					// 幅・高さ
			      0,					// border width
			      0,					// border color
			      WhitePixel(disp, DefaultScreen(disp)));	// background color

    XMapWindow(disp, win);

    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    GC gc = XCreateGC(disp, win, 0, NULL);

    XSetForeground(disp, gc,
		   BlackPixel(disp, DefaultScreen(disp)));

    // 暴露イベントを受け取る。
    XSelectInput(disp, win, ExposureMask);

    XFontStruct *font = XLoadQueryFont(disp, FONT_HELVETICA_14);
    XSetFont(disp, gc, font->fid);

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

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	if (ev.type != Expose) {
	    printf("%ld uninterested\n", time(NULL));
	    continue;
	} else {
	    printf("%ld expose event\n", time(NULL));
	}

	XClearWindow(disp, win);

	// 一文字ずつ描画せよ

	const int LEFT_MARGIN = 50;
	const int LINE_HEIGHT = 20;
	const int RIGHT_MARGIN = 500;
	const int TOP_MARGIN = 50;

	int i;
	int x = LEFT_MARGIN; // left margin
	int y = TOP_MARGIN + font->ascent;
	
	for (i = 0; msg[i] != '\0'; i++) {
	    XCharStruct *info = GetCharInfo(font, msg[i]);

	    if (x + info->width > RIGHT_MARGIN) {
		x = LEFT_MARGIN;
		y += LINE_HEIGHT;
	    }

	    XDrawString(disp, win, gc,
			x,	// X座標
			y,	// Y座標。ベースライン
			&msg[i],
			1);

	    // ずらす
	    x += info->width;
	}	
    }

    XUnloadFont(disp, font->fid);
    XDestroyWindow(disp, win);

    XCloseDisplay(disp);
}
