#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

XCharStruct *GetCharInfo(XFontStruct *font, unsigned char byte1, unsigned char byte2)
{
    // N = (max_byte2 - min_byte2 + 1) * (byte1 + min_byte1) - (byte2 - min_byte2)
    const int min_byte1 = font->min_byte1;
    const int max_byte1 = font->max_byte1;
    const int min_byte2 = font->min_char_or_byte2;
    const int max_byte2 = font->max_char_or_byte2;

    if ( !(byte1 >= min_byte1 && byte1 <= max_byte1 &&
	   byte2 >= min_byte2 && byte2 <= max_byte2) ) {
	abort();
    }

    const int index_into_per_char = 
	(max_byte2 - min_byte2 + 1) * (byte1 - min_byte1) + (byte2 - min_byte2);
    return &font->per_char[index_into_per_char];
}

void InspectCharStruct(XCharStruct character)  /* struct copy */
{
#define SHOW(fmt, member) printf("\t" #member " = " fmt "\n", character.member)
    SHOW("%hd", lbearing);
    SHOW("%hd", rbearing);
    SHOW("%hd", width);
    SHOW("%hd", ascent);
    SHOW("%hd", descent);
    SHOW("%hu", attributes);
#undef SHOW
}

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

    GC gc = XCreateGC(disp, win, 0, NULL);

    printf("%p\n", gc);

    XSetLineAttributes(disp, gc,
		       3,		// line_width
		       LineSolid,	// line_style, 
		       CapButt,		// cap_style
		       JoinMiter);	// join_style
    XSetForeground(disp, gc,
		   BlackPixel(disp, DefaultScreen(disp)));

    // 暴露イベントを要求する。
    XSelectInput(disp, win, ExposureMask);

    XFontStruct *font = XLoadQueryFont(disp, "k14");

    XEvent ev;
    const char *msg = "#L#o#r#e#m!!#i#p#s#u#m!!#d#o#l#o#r!!#s#i#t!!#a#m#e#t!$!!#c#o#n#s#e#c#t#e#t#u#r";

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	if (ev.type != Expose) {
	    printf("uninterested\n");
	    continue;
	} else {
	    printf("expose event\n");
	}

	XClearWindow(disp, win);

	// 一文字ずつ描画せよ

	XSetFont(disp, gc, font->fid);
	XChar2b *p = (XChar2b *) msg;
	int i;
	int x = 60; // initially left margin
	
	for (i = 0; i < strlen(msg) / 2; i++) {
	    XCharStruct *info = GetCharInfo(font, p[i].byte1, p[i].byte2);

	    InspectCharStruct(*info);
	    x += 1;
	    XDrawString16(disp, win, gc,
			  x - info->lbearing, // X座標
			  300  + font->ascent, // Y座標。ベースライン
			  p + i,
			  1);

	    if (info->rbearing - info->lbearing <= 0)
		x += info->width / 2;
	    else
		x += info->rbearing - info->lbearing + 1;
	    // ずらす
	}	

	// XDrawText16(disp, win, gc, 0, font->ascent + 2, &text_item, 1);

	XSetFont(disp, gc, font->fid);
	XDrawString16(disp, win, gc, 60, 280 + font->ascent, (XChar2b *) msg, strlen(msg) / 2);

	// XDrawLine(disp, win, gc, 5, y, 105, y);
	XFlush(disp); // 重要？ XNextEventでFlushされるような気もする
    }

    XUnloadFont(disp, font->fid);
    XDestroyWindow(disp, win);

    XCloseDisplay(disp);
}
