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
#include "font.h"
				   
static Display *disp;
static Window win;
static XdbeBackBuffer	 back_buffer;
YFont *font;

static char *text;
static Document *doc;

static CursorPath cursor_path;
static size_t top_line;

#define MAX_LINES 1024

static bool DRAW_BASELINE = 1;
static bool DRAW_LEADING = 0;
static bool DRAW_SPACE = 0;
static bool DRAW_NEWLINE = 0;
static bool MARK_MARGINS = 0;
static bool DRAW_EOF = 0;
static bool MARK_TOKENS = 0;

#define DEFAULT_FONT_DESC "Source Han Sans JP-16:matrix=1 0 0 1"
static const char *FONT_DESC = DEFAULT_FONT_DESC;

static short LINE_HEIGHT = -1;

// ファイルローカルな関数の宣言。
static char *InspectString(const char *str);
static char *InspectFcPattern(FcPattern *pat);
static char *InspectXftFont(XftFont *font);
static int LeadingAboveLine(XftFont *font);
static int LeadingBelowLine(XftFont *font);
static size_t DrawDocument(XftDraw *draw, Document *doc, size_t start);
static void DrawCursor(XftDraw *draw, short x, short y);
static void DrawLeadingAboveLine(XftDraw *draw, PageInfo *page, short y);
static void DrawLeadingBelowLine(XftDraw *draw, PageInfo *page, short y);
static void DrawBaseline(XftDraw *draw, PageInfo *page, short y);
static void DrawNewline(XftDraw *draw, short x, short y);
static void DrawSpace(XftDraw *draw, short x, short y, short width);
static void InspectXGlyphInfo(XGlyphInfo *extents);
static void DrawPrintableToken(XftDraw *draw, Token *tok, short left_margin, short y);
static void DrawEOF(XftDraw *draw, short x, short y);
static void DrawTab(XftDraw *draw, Token *tok, short margin_left, short y);
static void DrawToken(XftDraw *draw, Token *tok, PageInfo *page, short y);
static void DrawLineBefore(XftDraw *draw, PageInfo *page, short y);
static void DrawLine(XftDraw *draw, PageInfo *page, VisualLine *lines, size_t index, short y);
static void MarkMargins(PageInfo *page);
static void InitializeBackBuffer(void);

#define SET_OPTION_BOOL(param) if (streq(name, #param)) { param = (bool) atoi(value); goto Set; }
#define SET_OPTION_STRING(param) if (streq(name, #param)) { param = GC_STRDUP(value); goto Set; }
#define SET_OPTION_SHORT(param) if (streq(name, #param)) { param = (short) atoi(value); goto Set; }
void ViewSetOption(const char *name, const char *value)
{
    SET_OPTION_BOOL(DRAW_BASELINE);
    SET_OPTION_BOOL(DRAW_LEADING);
    SET_OPTION_BOOL(DRAW_SPACE);
    SET_OPTION_BOOL(DRAW_SPACE);
    SET_OPTION_BOOL(DRAW_NEWLINE);
    SET_OPTION_BOOL(MARK_MARGINS);
    SET_OPTION_BOOL(DRAW_EOF);
    SET_OPTION_BOOL(MARK_TOKENS);

    SET_OPTION_STRING(FONT_DESC);

    SET_OPTION_SHORT(LINE_HEIGHT);


    fprintf(stderr, "Warning: unknown option %s\n", InspectString(name));
    return;

 Set:
    // Redraw();
    return;
}
#undef SET_OPTION_BOOL
#undef SET_OPTION_STRING
#undef SET_OPTION_SHORT

// 行の上に置くべき行間を算出する。
static int LeadingAboveLine(XftFont *font)
{
    int lineSpacing = LINE_HEIGHT - (font->ascent + font->descent);

    return lineSpacing / 2;
}

static int LeadingBelowLine(XftFont *font)
{
    int lineSpacing = LINE_HEIGHT - (font->ascent + font->descent);

    // 1ピクセルの余りがあれば行の下に割り当てられる。
    return lineSpacing / 2 + lineSpacing % 2;
}

static void DrawCursor(XftDraw *draw, short x, short y)
{
    XftFont *xft_font = font->xft_font;

    // 行の高さのカーソル。
    XftDrawRect(draw, ColorGetXftColor("magenta"),
		x - 1, y - xft_font->ascent - LeadingAboveLine(xft_font),
		2, LINE_HEIGHT);
};

static void DrawLeadingAboveLine(XftDraw *draw, PageInfo *page, short y)
{
    XftFont *xft_font = font->xft_font;

    // 上の行間を描画する。
    if (DRAW_LEADING)
	XftDrawRect(draw, ColorGetXftColor("navajo white"),
		    page->margin_left, y - xft_font->ascent - LeadingAboveLine(xft_font),
		    page->margin_right - page->margin_left, LeadingAboveLine(xft_font));
}

static void DrawLeadingBelowLine(XftDraw *draw, PageInfo *page, short y)
{
    XftFont *xft_font = font->xft_font;

    // 下の行間を描画する。
    if (DRAW_LEADING)
	XftDrawRect(draw, ColorGetXftColor("cornflower blue"),
		    page->margin_left, y + xft_font->descent,
		    page->margin_right - page->margin_left, LeadingBelowLine(xft_font));
}

static void DrawBaseline(XftDraw *draw, PageInfo *page, short y)
{
    XftFont *xft_font = font->xft_font;

    // 下線。
    if (DRAW_BASELINE)
	XftDrawRect(draw, ColorGetXftColor("gray90"),
		    page->margin_left, y + xft_font->descent + LeadingBelowLine(xft_font),
		    page->margin_right - page->margin_left, 1);
}

#define NEWLINE_SYMBOL "↓"
static void DrawNewline(XftDraw *draw, short x, short y)
{
    if (DRAW_NEWLINE)
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("cyan4"),
			  font->xft_font,
			  x, y,
			  (FcChar8 *) NEWLINE_SYMBOL, sizeof(NEWLINE_SYMBOL) - 1);
}

static void DrawSpace(XftDraw *draw, short x, short y, short width)
{
    XftFont *xft_font = font->xft_font;

    if (DRAW_SPACE)
	XftDrawRect(draw, ColorGetXftColor("misty rose"),
		    x, y - xft_font->ascent,
		    width, xft_font->ascent + xft_font->descent);
}

static void InspectXGlyphInfo(XGlyphInfo *extents)
{
    printf("width = %hu\n", extents->width);
    printf("height = %hu\n", extents->height);
    printf("x = %hd\n", extents->x);
    printf("y = %hd\n", extents->y);
    printf("xOff = %hd\n", extents->xOff);
    printf("yOff = %hd\n", extents->yOff);
}

static void DrawPrintableToken(XftDraw *draw, Token *tok, short left_margin, short y)
{
    XftFont *xft_font = font->xft_font;

    for (int i = 0; i < tok->nchars; i++) {
	Character *ch = &tok->chars[i];

	int offset;
	if (Utf8IsAnyOf(ch->utf8, CC_OPEN_PAREN)) {
	    // 右寄せ。
	    int glyph_width = YFontTextWidth(font, ch->utf8, ch->length);
	    offset = -(glyph_width - ch->width);
	} else if (Utf8IsAnyOf(ch->utf8, CC_MIDDLE_DOT)) {
	    XGlyphInfo extents;
	    YFontTextExtents(font, ch->utf8, ch->length, &extents);
	    // offset = extents.x - extents.width / 2 + extents.xOff / 4;
	    offset =
		(ch->width - extents.width) / 2 + extents.x;
	} else {
	    offset = 0;
	}
	    

	XftDrawStringUtf8(draw,
			  ColorGetXftColor("black"),
			  xft_font,
			  left_margin + tok->x + ch->x + offset, y,
			  (FcChar8 *) ch->utf8,
			  strlen(ch->utf8));
    }
    if (MARK_TOKENS)
	// トークン区切りをあらわす下線を引く。
	XftDrawRect(draw, ColorGetXftColor("green4"),
		    left_margin + tok->x + 2, y + xft_font->descent + LeadingBelowLine(xft_font) - 1,
		    tok->width - 4, 2);
}

#define EOF_SYMBOL "[EOF]"

static void DrawEOF(XftDraw *draw, short x, short y)
{
    if (DRAW_EOF)
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("cyan4"),
			  font->xft_font,
			  x, y,
			  (FcChar8 *) EOF_SYMBOL,
			  sizeof(EOF_SYMBOL) - 1);
}

#define TAB_SYMBOL " "
static void DrawTab(XftDraw *draw, Token *tok, short margin_left, short y)
{
    XftDrawStringUtf8(draw,
		      ColorGetXftColor("cyan4"),
		      font->xft_font,
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
    XftFont *xft_font = font->xft_font;
    short y = doc->page->margin_top + LeadingAboveLine(xft_font) + xft_font->ascent;

    for (size_t i = start; i < doc->nlines; i++) {
	DrawLine(draw, doc->page, doc->lines, i, y);
	y += LINE_HEIGHT;

	short next_line_ink_bottom =
	    y + LeadingAboveLine(xft_font) + xft_font->ascent + xft_font->descent;
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

void ViewRedraw()
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


static void InitializeBackBuffer()
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

static char *InspectString(const char *str)
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

static char *InspectFcPattern(FcPattern *pat)
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

static char *InspectXftFont(XftFont *font)
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

void ViewInitialize(Display *aDisp, Window aWin, 
		    const char *aText, PageInfo *page)
{
    disp = aDisp;
    win = aWin;
    ColorInitialize(disp);
    InitializeBackBuffer();
    font = YFontCreate(disp, FONT_DESC);
    // フォントに設定されている高さを設定する。
    if (LINE_HEIGHT == -1) {
	LINE_HEIGHT = font->xft_font->height;
    }
    if (font == NULL) {
	fprintf(stderr, "no such font: %s\n", FONT_DESC);
	exit(1);
    }
    puts(InspectXftFont(font->xft_font));
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
