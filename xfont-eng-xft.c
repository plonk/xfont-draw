/**
 * Helvetica で英文を表示するプログラム。単語単位でラップする。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "util.h"
#include <ctype.h>

#define FONT_DESCRIPTION "Source Han Sans JP-20:matrix=1 0 0 1"

int LeadingBelowLine(XftFont *font);
int LeadingAboveLine(XftFont *font);

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

void Redraw(Display *disp, Window win, GC gc, XftFont *font, const char *msg)
{
    XftColor black;
    XftColorAllocName(disp,
		      DefaultVisual(disp,DefaultScreen(disp)),
		      DefaultColormap(disp,DefaultScreen(disp)),
		      "black", &black);
    // 解放しなくてもリークはしないはず。

    XftDraw *draw = XftDrawCreate(disp, win, DefaultVisual(disp,DefaultScreen(disp)), DefaultColormap(disp,DefaultScreen(disp)));

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

    XClearWindow(disp, win);
    size_t start = 0, next;
    int x = LEFT_MARGIN; // left margin
    int y = TOP_MARGIN + LeadingAboveLine(font) + font->ascent;
	
    while (NextToken(msg, start, &next)) {
	size_t len = next - start;
	int width = WordWidth(font, msg + start, len);

	if (x + width > RIGHT_MARGIN && // このトークンはこの行に入らない。
	    x != LEFT_MARGIN) { // 最初の単語単語が入らない場合ははみ出てもこの行に表示する。
	    x = LEFT_MARGIN;
	    y += LINE_HEIGHT;

	    if (isspace(msg[start])) // 空白位置で改行する場合は描画しない。
		goto nextIter;
	}
	XftDrawStringUtf8(draw, &black, font,
		    x,	// X座標
		    y,	// Y座標。ベースライン
		    msg + start,
		    len);
	x += width;

    nextIter:
	start = next;
    }
}

void Initialize(Window *pwin, GC *pgc, XftFont **font_return)
{

    disp = XOpenDisplay(NULL); // open $DISPLAY
    *pwin = XCreateSimpleWindow(disp,						// ディスプレイ
				DefaultRootWindow(disp),			// 親ウィンドウ
				0, 0,						// (x, y)
				640, 480,					// 幅・高さ
				0,						// border width
				0,						// border color
				WhitePixel(disp, DefaultScreen(disp)));	// background color
    XMapWindow(disp, *pwin);

    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    *pgc = XCreateGC(disp, *pwin, 0, NULL);
    XSetForeground(disp, *pgc,
		   BlackPixel(disp, DefaultScreen(disp)));

    // 暴露イベントを受け取る。
    XSelectInput(disp, *pwin, ExposureMask);
    
    *font_return = XftFontOpenName(disp, DefaultScreen(disp), FONT_DESCRIPTION);
}

void CleanUp(Display *disp, Window win, GC gc, XFontStruct *font)
{
    XUnloadFont(disp, font->fid);
    XFreeGC(disp, gc);
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}


// 行間は推奨されるフォントの高さ(height)−(ascent+descent)に等しく、
// 行の上部と下部に均等に配分される。
int LeadingAboveLine(XftFont *font)
{
    int lineSpacing = font->height - (font->ascent + font->descent);

    return lineSpacing / 2;
}

int LeadingBelowLine(XftFont *font)
{
    int lineSpacing = font->height - (font->ascent + font->descent);

    // 1ピクセルの余りがあれば行の下に割り当てられる。
    return lineSpacing / 2 + lineSpacing % 2;
}

int main()
{
    Window win;
    GC gc;
    XFontStruct *font;

    Initialize(&win, &gc, &font);

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

	Redraw(disp, win, gc, font, msg);
    }

    CleanUp(disp, win, gc, font);
}
