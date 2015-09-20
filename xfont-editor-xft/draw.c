#include <stdio.h>
#include <stdlib.h>

#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/CascadeB.h>
#include <Xm/DrawingA.h>
#include <Xm/RowColumn.h>

#include <gc.h>

#include "xfont-editor-xft-view.h"
#include "xfont-editor-xft.h"
#include "xfont-editor-xft-utf8.h"


void quit_call(void);
void draw_cbk(Widget , XtPointer ,
	      XmDrawingAreaCallbackStruct *);
void resize_cbk(Widget draw, XtPointer data,
		XmDrawingAreaCallbackStruct *cbk);
void load_font(XFontStruct **);

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

void keypress_callbck(Widget widget, XtPointer closure, XEvent *ev, Boolean *continue_to_dispatch)
{
    HandleKeyPress((XKeyEvent*) ev);
}

PageInfo *GetPageInfo(Display *disp, Window win)
{
    // ウィンドウのサイズを取得する。
    XWindowAttributes attrs;
    XGetWindowAttributes(disp, win, &attrs);

    PageInfo *page;

    page = GC_MALLOC(sizeof(PageInfo));
    page->width = attrs.width;
    page->height = attrs.height;

    page->margin_top = 50;
    page->margin_right = attrs.width - 50;
    page->margin_bottom = attrs.height - 50;
    page->margin_left = 50;

    return page;
}

void read_view_options(int *argc, char *argv[], const char* names[], bool values[])
{
    int j = 0;
    int k = 0;

    // -dOPTION -dOPTION=0 形式のオプションを認識する。
    for (int i = 0; i < *argc && k < 10; i++) {
	if (strncmp(argv[i], "-d", 2) == 0) {
	    const char *exp = argv[i] + 2;
	    const char *equals = strchr(exp, '=');

	    if (equals == NULL) {
		names[k] = GC_STRDUP(exp);
		values[k] = true;
	    } else {
		names[k] = GC_STRNDUP(exp, equals - exp);
		values[k] = atoi(equals + 1);
	    }
	    printf("%s is %d\n", names[k], values[k]);
	    k++;
	} else {
	    argv[j++] = argv[i];
	}
    }
    argv[j] = NULL;
    names[k] = NULL;
    *argc = j;
}

int main(int argc, char *argv[])
{
    Widget top_wid, main_w, menu_bar, draw;
    XtAppContext app;

    top_wid = XtVaAppInitialize(&app, "Draw", NULL, 0, 
				&argc, argv, NULL,
				XmNwidth,  500,
				XmNheight, 500,
				NULL);

    main_w = XtVaCreateManagedWidget("main_window",
				     xmMainWindowWidgetClass,   top_wid,
				     NULL);
        
    menu_bar = XmVaCreateSimpleMenuBar(main_w, "main_list",
				     XmVaCASCADEBUTTON,
				     XmStringCreateLocalized("ファイル"), 'F', NULL);
    XtManageChild(menu_bar);
      
    XmVaCreateSimplePulldownMenu(menu_bar, "file_menu", 0, 
				 (XtCallbackProc) quit_call,
				 XmVaPUSHBUTTON, XmStringCreateLocalized("終了"), 'Q', NULL, NULL, NULL);
        

    draw = XtVaCreateWidget("draw",
			    xmDrawingAreaWidgetClass, main_w,
			    NULL);
        
    XtVaSetValues(main_w,
		  XmNmenuBar,    menu_bar,
		  XmNworkWindow, draw,
		  NULL);

    XtVaSetValues(draw,
		  XmNresizePolicy, XmRESIZE_ANY,
		  NULL);
        
    XtAddCallback(draw, XmNexposeCallback, (XtCallbackProc) draw_cbk, NULL);
    XtAddCallback(draw, XmNresizeCallback, (XtCallbackProc) resize_cbk, NULL);

    XtManageChild(draw);
    XtRealizeWidget(top_wid);


    const char *names[10];
    bool values[10];
    read_view_options(&argc, argv, names, values);

    if (argc != 2) {
	fprintf(stderr, "Usage: draw FILENAME\n");
	exit(1);
    }
       
    const char *text = ReadFile(argv[1]);

    PageInfo *page;
    page = GetPageInfo(XtDisplay(draw), XtWindow(draw));
    ViewInitialize(XtDisplay(draw), XtWindow(draw), text, page);

    for (int i = 0; i < 10 && names[i] != NULL; i++) {
	ViewSetOption(names[i], values[i]);
    }
       
    XtAddEventHandler(draw, KeyPressMask, False, 
		      keypress_callbck, NULL);

    XtAppMainLoop(app);
}


void quit_call()
{
    printf("Quitting program\n");
    exit(0);
}

void
draw_cbk(Widget w, XtPointer data,
         XmDrawingAreaCallbackStruct *cbk)

{
    if (cbk->reason != XmCR_EXPOSE) {
	printf("X is screwed up!!\n");
	exit(1);
    } 

    Redraw();
}

void resize_cbk(Widget draw, XtPointer data,
		XmDrawingAreaCallbackStruct *cbk)
{
    if (XtWindow(draw) == 0) {
	return;
    } 
    PageInfo *page;
    page = GetPageInfo(XtDisplay(draw), XtWindow(draw));
    ViewSetPageInfo(page);
    Redraw();
}
