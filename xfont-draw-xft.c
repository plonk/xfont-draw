#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

Display* disp;

void InspectXftFont(XftFont *font)
{
    printf("ascent = %d\n", font->ascent);
    printf("descent = %d\n", font->descent);
    printf("height = %d\n", font->height);
    printf("max_advance_width = %d\n", font->max_advance_width);
    printf("charset = %p\n", font->charset);
    printf("pattern = %p\n", font->pattern);
}

void InspectXGlyphInfo(XGlyphInfo *extents)
{
    printf("width = %hu\n", extents->width);
    printf("height = %hu\n", extents->height);
    printf("x = %hd\n", extents->x);
    printf("y = %hd\n", extents->y);
    printf("xOff = %hd\n", extents->xOff);
    printf("yOff = %hd\n", extents->yOff);
}

void GetGlyphInfo(char ch, XftFont *font, XGlyphInfo *extents_return)
{
    char str[7] = ""; // 最長のUTF8文字が入る大きさを確保する。

    str[0] = ch;
    XftTextExtentsUtf8(disp, font, (FcChar8 *) str, 1, extents_return);
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
    // ディスプレイ
    disp = XOpenDisplay(NULL); // open $DISPLAY

    // ウィンドウ
    Window win;
    win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 640, 480, 0, 0, WhitePixel(disp, DefaultScreen(disp)));
    XMapWindow(disp, win);
    XSelectInput(disp, win, ExposureMask);

    // Xftドロー
    XftDraw *draw = XftDrawCreate(disp, win, DefaultVisual(disp,DefaultScreen(disp)), DefaultColormap(disp,DefaultScreen(disp)));

    // Xftカラー
    XftColor color;
    XftColorAllocName(disp, DefaultVisual(disp,DefaultScreen(disp)), DefaultColormap(disp,DefaultScreen(disp)),
		      "black", &color);

    // Xftフォント
    XftFont *font = XftFontOpenName(disp, DefaultScreen(disp), "Source Han Sans JP-20:matrix=1 0 0 1");

    InspectXftFont(font);

    XEvent ev;
    const char *msg = "Lorem ipsum dolor sit amet, consectetur";

    int LeftMargin = 50;
    int TopMargin = 50;

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	if (ev.type != Expose) {
	    printf("uninterested\n");
	    continue;
	} else {
	    printf("expose event\n");
	}

	XClearWindow(disp, win);

	int y = TopMargin + LeadingAboveLine(font) + font->ascent; // ベースライン
	for (int j = 0; j < 3; j++) {
	    // 一文字ずつ描画せよ
	    int i;
	    int x = LeftMargin; // left margin
	
	    for (i = 0; i < strlen(msg); i++) {
		XGlyphInfo info;

		GetGlyphInfo(msg[i], font, &info);

		InspectXGlyphInfo(&info);

		XftDrawStringUtf8(draw, &color, font,
				  x, y,
				  (unsigned char *) &msg[i], 1);
		x += info.xOff;
	    }	
	    y += font->height;
	}
    }

    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}
