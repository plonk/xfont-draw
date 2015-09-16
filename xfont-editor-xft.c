/**
 * カーソル移動ができるテキストエディタ。
 */

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <gc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>

#include "color.h"
#include "util.h"
#include "xfont-editor-xft-utf8.h"
#include "xfont-editor-xft.h"
#include "xfont-editor-xft-view.h"

#define FONT_DESCRIPTION "Source Han Sans JP-16:matrix=1 0 0 1"

Display *disp;
Window win;
XdbeBackBuffer	 back_buffer;
XftFont *font;

void CleanUp();

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

void Initialize()
{

    disp = XOpenDisplay(NULL); // open $DISPLAY

    ColorInitialize(disp);

    win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 640, 480, 0, 0, WhitePixel(disp, DefaultScreen(disp)));	

    InitializeBackBuffer();

    // awesome ウィンドウマネージャーの奇妙さかもしれないが、マップす
    // る前にプロトコルを登録しないと delete 時に尊重されないので、こ
    // のタイミングで登録する。
    Atom WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False); 
    XSetWMProtocols(disp, win, &WM_DELETE_WINDOW, 1);

    XMapWindow(disp, win);
    // 暴露イベントを受け取る。
    XSelectInput(disp, win, ExposureMask | KeyPressMask);

    font = XftFontOpenName(disp, DefaultScreen(disp), FONT_DESCRIPTION);

    atexit(CleanUp);
}

void CleanUp()
{
    fprintf(stderr, "cleanup\n");
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}

void HandleKeyPress(XKeyEvent *ev)
{
    bool needs_redraw = false;

    KeySym sym;
    sym = XLookupKeysym(ev, 0);

    switch (sym) {
    case XK_Right:
	needs_redraw = ViewForwardCursor();
	break;
    case XK_Left:
	needs_redraw = ViewBackwardCursor();
	break;
    case XK_Up:
	needs_redraw = ViewUpwardCursor();
	break;
    case XK_Down:
	needs_redraw = ViewDownwardCursor();
	break;
    }

    if (needs_redraw) {
	Redraw();
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
	fprintf(stderr, "Usage: %s FILENAME\n", argv[0]);
	exit(1);
    }

    Initialize();

    const char *text = ReadFile(argv[1]);

    PageInfo page;
    GetPageInfo(&page);
    ViewInitialize(text, &page);

    XEvent ev;
    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	switch (ev.type) {
	case Expose:
	    puts("expose");
	    GetPageInfo(&page);
	    SetPageInfo(&page);
	    Redraw();
	    break;
	case KeyPress:
	    puts("keypress");
	    HandleKeyPress((XKeyEvent *) &ev);
	    break;
	case ClientMessage:
	    exit(0);
	}
    }
}
