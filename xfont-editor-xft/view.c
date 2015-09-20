#include <stdbool.h>
#include <gc.h>

#include "util.h"
#include "color.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>

#include "utf8-string.h"
#include "view.h"
#include "document.h"

static Display *disp;
static Window win;
static XdbeBackBuffer	 back_buffer;
XftFont *font;

static char *text;
static Document *doc;

static CursorPath cursor_path;
static size_t top_line;

#define MAX_LINES 1024

// 行の上に置くべき行間を算出する。
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

int TextWidthUncached(XftFont *font, const char *str, int bytes)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(disp, font, (FcChar8 *) str, bytes, &extents);
    return extents.xOff;
}

static bool DRAW_BASELINE = 1;
static bool DRAW_LEADING = 0;
static bool DRAW_SPACE = 0;
static bool DRAW_NEWLINE = 0;
static bool MARK_MARGINS = 0;
static bool DRAW_EOF = 0;
static bool MARK_TOKENS = 0;

void ViewSetOption(const char *name, bool b)
{
#define SET_OPTION(param) if (streq(name, #param)) { param = b; goto Set; }

    SET_OPTION(DRAW_BASELINE);
    SET_OPTION(DRAW_LEADING);
    SET_OPTION(DRAW_SPACE);
    SET_OPTION(DRAW_SPACE);
    SET_OPTION(DRAW_NEWLINE);
    SET_OPTION(MARK_MARGINS);
    SET_OPTION(DRAW_EOF);
    SET_OPTION(MARK_TOKENS);

#undef SET_OPTION

    fprintf(stderr, "Warning: unknown option %s\n", InspectString(name));
    return;

 Set:
    Redraw();
    return;
}

static void DrawCursor(XftDraw *draw, short x, short y)
{
    // 行の高さのカーソル。
    XftDrawRect(draw, ColorGetXftColor("magenta"),
		x - 1, y - font->ascent - LeadingAboveLine(font),
		2, font->height);
};

static void DrawLeadingAboveLine(XftDraw *draw, PageInfo *page, short y)
{
    // 上の行間を描画する。
    if (DRAW_LEADING)
	XftDrawRect(draw, ColorGetXftColor("navajo white"),
		    page->margin_left, y - font->ascent - LeadingAboveLine(font),
		    page->margin_right - page->margin_left, LeadingAboveLine(font));
}

static void DrawLeadingBelowLine(XftDraw *draw, PageInfo *page, short y)
{
    // 下の行間を描画する。
    if (DRAW_LEADING)
	XftDrawRect(draw, ColorGetXftColor("cornflower blue"),
		    page->margin_left, y + font->descent,
		    page->margin_right - page->margin_left, LeadingBelowLine(font));
}

static void DrawBaseline(XftDraw *draw, PageInfo *page, short y)
{
    // 下線。
    if (DRAW_BASELINE)
	XftDrawRect(draw, ColorGetXftColor("gray90"),
		    page->margin_left, y + font->descent + LeadingBelowLine(font),
		    page->margin_right - page->margin_left, 1);
}

#define NEWLINE_SYMBOL "↓"
static void DrawNewline(XftDraw *draw, short x, short y)
{
    if (DRAW_NEWLINE)
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("cyan4"),
			  font,
			  x, y,
			  (FcChar8 *) NEWLINE_SYMBOL, sizeof(NEWLINE_SYMBOL) - 1);
}

static void DrawSpace(XftDraw *draw, short x, short y, short width)
{
    if (DRAW_SPACE)
	XftDrawRect(draw, ColorGetXftColor("misty rose"),
		    x, y - font->ascent,
		    width, font->ascent + font->descent);
}

static void DrawPrintableToken(XftDraw *draw, Token *tok, short left_margin, short y)
{
    for (int i = 0; i < tok->nchars; i++) {
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("black"),
			  font,
			  left_margin + tok->x + tok->chars[i].x, y,
			  (FcChar8 *) tok->chars[i].utf8,
			  strlen(tok->chars[i].utf8));
	if (MARK_TOKENS)
	    // トークン区切りをあらわす下線を引く。
	    XftDrawRect(draw, ColorGetXftColor("green4"),
			left_margin + tok->x + 2, y + font->descent + LeadingBelowLine(font) - 1,
			tok->width - 4, 2);
    }
}

#define EOF_SYMBOL "[EOF]"

static void DrawEOF(XftDraw *draw, short x, short y)
{
    if (DRAW_EOF)
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("cyan4"),
			  font,
			  x, y,
			  (FcChar8 *) EOF_SYMBOL,
			  sizeof(EOF_SYMBOL) - 1);
}

#define TAB_SYMBOL " "
static void DrawTab(XftDraw *draw, Token *tok, short margin_left, short y)
{
    XftDrawStringUtf8(draw,
		      ColorGetXftColor("cyan4"),
		      font,
		      margin_left + tok->x,
		      y,
		      (FcChar8 *) TAB_SYMBOL,
		      sizeof(TAB_SYMBOL) - 1);
}

static void DrawToken(XftDraw *draw, Token *tok, PageInfo *page, short y)
{
    if (TokenIsEOF(tok)) {
	DrawEOF(draw, page->margin_left + tok->x, y);
    } else {
	switch (tok->chars[0].utf8[0]) {
	case ' ':
	    DrawSpace(draw, page->margin_left + tok->x, y, tok->width);
	    break;
	case '\n':
	    DrawNewline(draw, page->margin_left + tok->x, y);
	    break;
	case '\t':
	    DrawTab(draw, tok, page->margin_left, y);
	    break;
	default:
	    // 普通の文字からなるトークン
	    DrawPrintableToken(draw, tok, page->margin_left, y);
	}
    }
}

// 行を描画する前に実行する。
static void DrawLineBefore(XftDraw *draw, PageInfo *page, short y)
{
    DrawLeadingAboveLine(draw, page, y);
    DrawLeadingBelowLine(draw, page, y);
    DrawBaseline(draw, page, y);
}

static void DrawLine(XftDraw *draw, PageInfo *page, VisualLine *lines, size_t index, short y)
{
    VisualLine *line = &lines[index];

    DrawLineBefore(draw, page, y);

    // 行の描画
    for (int i = 0; i < line->ntokens; i++) {
	Token *tok = &line->tokens[i];

	DrawToken(draw, tok, page, y);

	// カーソルを描画する。
	for (int j = 0; j < tok->nchars; j++) {
	    if (CursorPathEquals(cursor_path, (CursorPath) { index, i, j }))
		DrawCursor(draw, page->margin_left + tok->x + tok->chars[j].x, y);
	}
    }
}

static size_t DrawDocument(XftDraw *draw, Document *doc, size_t start)
{
    short y = doc->page->margin_top + LeadingAboveLine(font) + font->ascent;

    for (size_t i = start; i < doc->nlines; i++) {
	DrawLine(draw, doc->page, doc->lines, i, y);
	y += font->height;

	short next_line_ink_bottom = y + LeadingAboveLine(font) + font->ascent + font->descent;
	if (next_line_ink_bottom > doc->page->margin_bottom)
	    return i;
    }
    return doc->nlines - 1;
}

static void MarkMargins(PageInfo *page)
{
    // ページのサイズ。
    const int lm	= page->margin_left   - 1;
    const int rm	= page->margin_right  + 1;
    const int tm	= page->margin_top    - 1;
    const int bm	= page->margin_bottom + 1;

    // マークを構成する線分の長さ。
    const int len = 10;

    GC gc;
    gc = XCreateGC(disp, back_buffer, 0, NULL);
    XSetForeground(disp, gc, ColorGetPixel("gray80"));

    // _|
    XDrawLine(disp, back_buffer, gc, lm - len, tm, lm, tm); // horizontal
    XDrawLine(disp, back_buffer, gc, lm, tm - len, lm, tm); // vertical

    //        |_
    XDrawLine(disp, back_buffer, gc, rm, tm, rm + len, tm);
    XDrawLine(disp, back_buffer, gc, rm, tm - len, rm, tm);

    // -|
    XDrawLine(disp, back_buffer, gc, lm - len, bm, lm, bm);
    XDrawLine(disp, back_buffer, gc, lm, bm, lm, bm + len);

    //        |-
    XDrawLine(disp, back_buffer, gc, rm, bm, rm + len, bm);
    XDrawLine(disp, back_buffer, gc, rm, bm, rm, bm + len);

    XFreeGC(disp, gc);
}

void Redraw()
{
    if (doc->page->width < doc->page->margin_left * 2) {
	fprintf(stderr, "Viewport size too small.\n");
	return;
    }

    XftDraw *draw = XftDrawCreate(disp, back_buffer,
			     DefaultVisual(disp,DefaultScreen(disp)),
			     DefaultColormap(disp,DefaultScreen(disp)));

 Retry:
    XftDrawRect(draw, ColorGetXftColor("white"), 0, 0, doc->page->width, doc->page->height);

    if (MARK_MARGINS)
	MarkMargins(doc->page);

    size_t last_line = DrawDocument(draw, doc, top_line);

    if (cursor_path.line > last_line) {
	top_line++;
	goto Retry;
    } else if (cursor_path.line < top_line) {
	top_line = cursor_path.line;
	goto Retry;
    }

    // フロントバッファーとバックバッファーを入れ替える。
    // 操作後、バックバッファーの内容は未定義になる。
    XdbeSwapBuffers(disp, &(XdbeSwapInfo) { win, XdbeUndefined }, 1);

    XftDrawDestroy(draw);
}


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

char *InspectString(const char *str)
{
    // 一部の制御文字をバックスラッシュ表現にエスケープする。
#define BACKSLASH "\\"
    static const char *table[][2] = {
	{"\\", BACKSLASH "\\" },
	{"\"", BACKSLASH "\"" },
	{"\'", BACKSLASH "\'" },
	{"\n", BACKSLASH "n" },
	{"\r", BACKSLASH "r" },
	{"\b", BACKSLASH "b" },
	{"\t", BACKSLASH "t" },
	{"\f", BACKSLASH "f" },
	{"\a", BACKSLASH "a" },
	{"\v", BACKSLASH "v" }
    };
#undef BACKSLASH
    // 最大で元の文字列の 2 倍の長さになりうるので、変換先の文字列を保
    // 待する領域をその大きさで確保する。
    char escaped[strlen(str) * 2 + 1];
    const char *in = str;
    char *out = escaped;

    while (*in != '\0') {
	int nitems = sizeof(table) / sizeof(table[0]);
	for (int i = 0; i < nitems; i++) {
	    // 変換元の文字列は 1 バイトなので strncmp の第三引数は 1
	    // でよい。
	    if (strncmp(in, table[i][0], 1) == 0) {
		strcpy(out, table[i][1]);
		out += strlen(table[i][1]);
		goto NextChar;
	    }
	}
	// その他の制御文字を ^X の形にエスケープする。
	if ((unsigned char) *in <= 0x1f) {
	    *out++ = '^';
	    *out++ = *in | 0x40;
	    goto NextChar;
	}
	*out++ = *in;
    NextChar:
	in++;
    }
    *out = '\0';

    char *res = StringConcat((const char* []) { "\"", escaped, "\"", NULL });
    return res;
}

char *InspectFcPattern(FcPattern *pat)
{
#define STRING_PROPERTY(name) ({ FcChar8 *s; FcPatternGetString(pat, name, 0, &s); Format(name "=%s ", InspectString((char*)s)); })
#define DOUBLE_PROPERTY(name) ({ double d; FcPatternGetDouble(pat, name, 0, &d); Format(name "=%g ", d); })

    char *res = StringConcat((const char* []) {
	Format("#<FcPattern:%p ", pat),
	STRING_PROPERTY(FC_FAMILY),
	STRING_PROPERTY(FC_STYLE),
	DOUBLE_PROPERTY(FC_SIZE),
	DOUBLE_PROPERTY(FC_PIXEL_SIZE),
	NULL
    });
    res[strlen(res) - 1] = '>';
    return res;
#undef STRING_PROPERTY
#undef DOUBLE_PROPERTY
}
char *InspectXftFont(XftFont *font)
{
    return Format("#<XftFont:%p "
		 "ascent=%d "
		 "descent=%d "
		 "height=%d "
		 "max_adavnce_width=%d "
		 "pattern=%s>",
		 font,			
		 font->ascent,		
		 font->descent,		
		 font->height,		
		 font->max_advance_width,
		 InspectFcPattern(font->pattern));
}

#define FONT_DESCRIPTION "Source Han Sans JP-16:matrix=1 0 0 1"

void ViewInitialize(Display *aDisp, Window aWin, 
		    const char *aText, PageInfo *page)
{
    disp = aDisp;
    win = aWin;
    ColorInitialize(disp);
    InitializeBackBuffer();
    font = XftFontOpenName(disp, DefaultScreen(disp), FONT_DESCRIPTION);
    puts(InspectXftFont(font));
    text = GC_STRDUP(aText);
    cursor_path = (CursorPath) { 0, 0, 0 };
    doc = CreateDocument(text, page);
}

void ViewSetPageInfo(PageInfo *page)
{
    size_t offset = CursorPathToCharacterOffset(doc, cursor_path);
    DocumentSetPageInfo(doc, page);
    cursor_path = ToCursorPath(doc, offset);
}

// カーソルを一文字先に進める。状態が変更されたら true を返す。
bool ViewForwardCursor()
{
    CursorPath newLoc = CursorPathForward(doc, cursor_path);

    if (CursorPathEquals(newLoc, cursor_path))
	return false;
    else {
	cursor_path = newLoc;
	return true;
    }
}

// カーソルを一文字前に戻す。状態が変更されたら true を返す。
bool ViewBackwardCursor()
{
    CursorPath newLoc = CursorPathBackward(doc, cursor_path);

    if (CursorPathEquals(newLoc, cursor_path))
	return false;
    else {
	cursor_path = newLoc;
	return true;
    }
}

// カーソルを一行上に戻す。状態が変更されたら true を返す。
bool ViewUpwardCursor()
{
    if (cursor_path.line == 0)
	return false;

    short preferred_x = CursorPathGetX(doc, cursor_path);
    CursorPath it = { cursor_path.line, 0, 0 }; // 行頭へ移動する。

    short x;
    do {
	it = CursorPathBackward(doc, it);
	x = CursorPathGetX(doc, it);
    } while (x > preferred_x);

    cursor_path = it;

    return true;
}

// カーソルを一行下へ進める。状態が変更されたら true を返す。
bool ViewDownwardCursor()
{
    if (cursor_path.line == doc->nlines - 1)
	return false;

    short preferred_x = CursorPathGetX(doc, cursor_path);
    CursorPath it = { cursor_path.line + 1, 0, 0 }; // 次の行の行頭へ移動する。
    CursorPath target;

    do {
	target = it;
	if (CursorPathGetCharacter(doc, it)->length == 0)
	    break;
	it = CursorPathForward(doc, it);
    } while (it.line == cursor_path.line + 1 && CursorPathGetX(doc, it) <= preferred_x);

    cursor_path = target;

    return true;
}
