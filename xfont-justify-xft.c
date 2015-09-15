/**
 * Helvetica で英文を表示するプログラム。両端揃え。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "util.h"
#include <X11/Xft/Xft.h>

#include <ctype.h>
#include <assert.h>

#define FONT_DESCRIPTION "Source Han Sans JP-20:matrix=1 0 0 1"

int LeadingAboveLine(XftFont *font);

int LeadingAboveLine(XftFont *font)
{
    int lineSpacing = font->height - (font->ascent + font->descent);

    return lineSpacing / 2;
}

Display *disp;

void GetGlyphInfo(char ch, XftFont *font, XGlyphInfo *extents_return)
{
    char str[7] = ""; // 最長のUTF8文字が入る大きさを確保する。

    str[0] = ch;
    XftTextExtentsUtf8(disp, font, (FcChar8 *) str, 1, extents_return);
}

int WordWidth(XftFont *font, const char *str, int len)
{
    int i;
    int width = 0;

    for (i = 0; i < len; i++) {
	XGlyphInfo info;

	GetGlyphInfo(str[i], font, &info);
	width += info.xOff;
    }
    return width;
}

struct Token {
    int width;
    size_t start;
    size_t length;
};

void DrawLine(XftDraw *draw, XftColor *color, XftFont *font, int LEFT_MARGIN, int y, const char *msg, struct Token tokens[], int ntokens) {
    // 行の描画
    puts("DrawLine");
    int l;
    int x = LEFT_MARGIN;
    for (l = 0; l < ntokens; l++) {
	if (isspace(msg[tokens[l].start])) {
	    x += tokens[l].width;
	} else {
	    XftDrawStringUtf8(draw, color, font,
			      x,	// X座標
			      y,	// Y座標。ベースライン
			      msg + tokens[l].start,
			      tokens[l].length);
	    x += tokens[l].width;
	}
    }
}

void Redraw(Display *disp, Window win, XftFont *font,
	    const char *msg)
{
    // 「黒」を割り当てる。毎回解放しなくてもリークはしないはず。
    XftColor black;
    XftColorAllocName(disp,
		      DefaultVisual(disp,DefaultScreen(disp)),
		      DefaultColormap(disp,DefaultScreen(disp)),
		      "black", &black);

    // ウィンドウのサイズを取得する。
    XWindowAttributes attrs;
    XGetWindowAttributes(disp, win, &attrs);

    const int LEFT_MARGIN = 50;
    const int LINE_HEIGHT = font->height;
    const int RIGHT_MARGIN = attrs.width - LEFT_MARGIN;
    const int TOP_MARGIN = 50;

    if (RIGHT_MARGIN < 10) {
	fprintf(stderr, "Viewport size too small.\n");
	return;
    }

    XftDraw *draw = XftDrawCreate(disp, win, DefaultVisual(disp,DefaultScreen(disp)), DefaultColormap(disp,DefaultScreen(disp)));

    XClearWindow(disp, win);
    size_t start = 0, next;
    int x = LEFT_MARGIN; // left margin
    int y = TOP_MARGIN + LeadingAboveLine(font) + font->ascent;

    int SPACE_STRETCH_LIMIT;
    {
	XGlyphInfo extents;
	GetGlyphInfo(' ', font, &extents);
	SPACE_STRETCH_LIMIT = extents.xOff * 2;
    }

#define MAX_TOKENS_PER_LINE 1024
    struct Token tokens[MAX_TOKENS_PER_LINE];
    int ntok = 0;
    /**
     * トークンを切り出して、RIGHT_MARGIN が埋まる直前まで tokens にトークンの情報を入れる。
     * 次に空白を表わすトークンが不足分(RIGHT_MARGIN - x)を補うように引き伸ばす。
     * 行を表示して y を増やす。tokens をクリアする。
     * 入らなかったトークンが空白だった場合は省略、それ以外の場合は tokens[0] とする。
     */
    while (NextToken(msg, start, &next)) {
	size_t len = next - start;
	int width = WordWidth(font, msg + start, len);

    redoToken:
	// 行頭の空白トークンを無視する
	if (isspace(msg[start]) && x == LEFT_MARGIN)
		goto nextIter;

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
		for (j = 0; j < nspaces; j++) {
		    if (plus_alphas[j] > SPACE_STRETCH_LIMIT)
			plus_alphas[j] = SPACE_STRETCH_LIMIT;
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

	    DrawLine(draw, &black, font, LEFT_MARGIN, y, msg, tokens, ntok);

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
    DrawLine(draw, &black, font, LEFT_MARGIN, y, msg, tokens, ntok);
}

void Initialize(Window *win_return, XftFont **font_return)
{

    disp = XOpenDisplay(NULL); // open $DISPLAY
    *win_return = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 640, 480, 0, 0, WhitePixel(disp, DefaultScreen(disp)));	
    XMapWindow(disp, *win_return);
    // 暴露イベントを受け取る。
    XSelectInput(disp, *win_return, ExposureMask);

    *font_return = XftFontOpenName(disp, DefaultScreen(disp), FONT_DESCRIPTION);
}

void CleanUp(Display *disp, Window win, XftFont *font)
{
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}

int main()
{
    Window win;
    XftFont *font;

    Initialize(&win,&font);

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

	if (ev.type != Expose)
	    continue;

	Redraw(disp, win, font, msg);
    }

    CleanUp(disp, win, font);
}
