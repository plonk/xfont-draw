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

#define FONT_HELVETICA_14 "-adobe-helvetica-medium-r-*-*-14-*-*-*-*-*-iso8859-1"

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

struct Token {
    int width;
    size_t start;
    size_t length;
};

void DrawLine(Display *disp, Window win, GC gc,
	      int LEFT_MARGIN, int y, const char *msg, struct Token tokens[], int ntokens) {
    // 行の描画
    puts("DrawLine");
    int l;
    int x = LEFT_MARGIN;
    for (l = 0; l < ntokens; l++) {
	if (isspace(msg[tokens[l].start])) {
	    x += tokens[l].width;
	} else {
	    XDrawString(disp, win, gc,
			x,	// X座標
			y,	// Y座標。ベースライン
			msg + tokens[l].start,
			tokens[l].length);
	    x += tokens[l].width;
	}
    }
}

void Redraw(Display *disp, Window win, GC gc, XFontStruct *font,
	    const char *msg)
{
    XWindowAttributes attrs;

    XGetWindowAttributes(disp, win, &attrs);

    const int LEFT_MARGIN = 50;
    const int LINE_HEIGHT = 20;
    const int RIGHT_MARGIN = attrs.width - LEFT_MARGIN;
    const int TOP_MARGIN = 50;

    if (RIGHT_MARGIN < 10) {
	fprintf(stderr, "Viewport size too small.\n");
	return;
    }

    XClearWindow(disp, win);
    size_t start = 0, next;
    int x = LEFT_MARGIN; // left margin
    int y = TOP_MARGIN + font->ascent;

#define MAX_TOKENS_PER_LINE 1024
    struct Token tokens[MAX_TOKENS_PER_LINE];
    int ntok = 0;
    /**
     * トークンを切り出して、LEFT_MARGIN が埋まる直前まで tokens にトークンの情報を入れる。
     * 次に空白を表わすトークンが不足分(LEFT_MARGIN - x)を補うように引き伸ばす。
     * 行を表示して y を増やす。tokens をクリアする。
     * 入らなかったトークンが空白だった場合は省略、それ以外の場合は tokens[0] とする。
     */
    while (NextToken(msg, start, &next)) {
	size_t len = next - start;
	int width = WordWidth(font, msg + start, len);

    redoToken:
	if (isspace(msg[start])) { // 空白トークン
	    if (x == LEFT_MARGIN) // 行頭
		goto nextIter; // 無視する
	    else {
		goto tryAddToken;
	    }
	}
    tryAddToken:
	printf("tryAddToken: %d\n", (int) start);
	printf("tryAddToken: x = %d\n", x);
	if (x + width > RIGHT_MARGIN && // 入らない
	    x != LEFT_MARGIN) { // 行の最初のトークンの場合は見切れてもよい
	    // 行の完成

	    assert(ntok > 0);

	    // 行末の空白を削除する
	    if (isspace(msg[tokens[ntok - 1].start])) {
		x -= tokens[ntok - 1].width;
		ntok--;
	    }

	    // 行の空白の数を数える。
	    int nspaces = 0;
	    int m;
	    for (m = 0; m < ntok; m++) {
		if (isspace(msg[tokens[m].start]))
		    nspaces++;
	    }

	    // 空白の調整
	    if (nspaces > 0) {
		printf("%d spaces\n", nspaces);
		const int shortage = RIGHT_MARGIN - x;
		int plus_alphas[MAX_TOKENS_PER_LINE];
		int j, k;

		Distribute(shortage, nspaces, plus_alphas);
		const int MAX_ADDEND = 8;
		for (j = 0; j < nspaces; j++) {
		    if (plus_alphas[j] > MAX_ADDEND)
			plus_alphas[j] = MAX_ADDEND;
		}
		
		k = 0;
		for (j = 0; j < ntok; j++) {
		    if (isspace(msg[tokens[j].start])) {
			printf("adding %d to tokens[%d].width\n", plus_alphas[k], j);
			tokens[j].width += plus_alphas[k++];
		    }
		}
		assert(k == nspaces);
	    }

	    DrawLine(disp, win, gc, LEFT_MARGIN, y, msg, tokens, ntok);

	    y += LINE_HEIGHT;
	    x = LEFT_MARGIN;
	    ntok = 0; // トークン配列のクリア
	    goto redoToken;
	} else {	
	    printf("AddToken '%c...'\n", msg[start]);
	    // トークンを追加する。
	    tokens[ntok].width = width;
	    tokens[ntok].start = start;
	    tokens[ntok].length = len;
	    x += width;
	    ntok++;
	}
    nextIter:
	start = next;
    }
    DrawLine(disp, win, gc, LEFT_MARGIN, y, msg, tokens, ntok);
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

    const char *harry = "Mr and Mrs Dursley, of number four, Privet Drive, were proud to say that they were perfectly normal, thank you very much. They were the last people you'd expect to be involved in anything strange or mysterious, because they just didn't hold with such nonsense.";

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	if (ev.type != Expose)
	    continue;

	Redraw(disp, win, gc, font, harry);
    }

    CleanUp(disp, win, gc, font);
}
