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

#include "util.h"
#include "utf8-string.h"
#include "editor.h"
#include "view.h"

static Display *disp;
static Window win;

void CleanUp();

void Initialize()
{

    disp = XOpenDisplay(NULL); // open $DISPLAY

    win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 640, 480, 0, 0, WhitePixel(disp, DefaultScreen(disp)));	

    // awesome ウィンドウマネージャーの奇妙さかもしれないが、マップす
    // る前にプロトコルを登録しないと delete 時に尊重されないので、こ
    // のタイミングで登録する。
    Atom WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False); 
    XSetWMProtocols(disp, win, &WM_DELETE_WINDOW, 1);

    XMapWindow(disp, win);
    // 暴露イベントを受け取る。
    XSelectInput(disp, win, ExposureMask | KeyPressMask);

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

void GetPageInfo(PageInfo *page)
{
    // ウィンドウのサイズを取得する。
    XWindowAttributes attrs;
    XGetWindowAttributes(disp, win, &attrs);

    page->width = attrs.width;
    page->height = attrs.height;

    page->margin_top = 50;
    page->margin_right = attrs.width - 50;
    page->margin_bottom = attrs.height - 50;
    page->margin_left = 50;
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
    ViewInitialize(disp, win, text, &page);

    XEvent ev;
    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	switch (ev.type) {
	case Expose:
	    puts("expose");
	    GetPageInfo(&page);
	    ViewSetPageInfo(&page);
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
