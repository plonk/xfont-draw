/**
 * Helvetica で英文を表示するプログラム。両端揃え。
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

#define FONT_DESCRIPTION "Source Han Sans JP-16:matrix=1 0 0 1"

Display *disp;
Window win;
XdbeBackBuffer	 back_buffer;
XftFont *font;

size_t cursor_offset;
size_t top_line;

typedef struct {
    size_t line;
    size_t token;
    size_t character;
} CursorPath;

CursorPath cursor_path;

typedef struct {
    short x;
    short width;
    char *utf8;
} Character;

typedef struct {
    short x;
    short width;
    Character *chars;
    size_t nchars;
} Token;

typedef struct  {
    Token *tokens;
    size_t ntokens;
} VisualLine;

typedef struct {
    short width, height;
    short margin_top, margin_right, margin_bottom, margin_left;
} PageInfo;

typedef struct {
    VisualLine *lines;
    size_t nlines;
    PageInfo *page;
    size_t max_offset;
} Document;

Document *doc;

#define MAX_LINES 1024

void CleanUp();
int WordWidth(XftFont *font, const char *str, int len);
const char *Utf8AdvanceChar(const char *utf8);
static inline size_t Utf8CharBytes(const char *utf8);

bool CursorPathEquals(CursorPath a, CursorPath b)
{
    return (a.line == b.line &&
	    a.token == b.token &&
	    a.character == b.character);
}

// str の  start 位置からトークン(単語あるいは空白)を切り出す。
// トークンを構成する最後の文字の次の位置が *end に設定される。
// トークンが読み出せた場合は 1、さもなくば 0 を返す。
int NextTokenBilingual(const char *utf8, size_t start, size_t *end)
{
    const char *p = utf8 + start;

    /* トークナイズするものがない */
    if (*p == '\0')
	return 0;

    if (*p == ' ') {
	do {
	    p = Utf8AdvanceChar(p);
	} while (*p && *p == ' ');
    } else if (Utf8CharBytes(p) == 1 && *p >= 0x21 && *p <= 0x7e) {
	do  {
	    p = Utf8AdvanceChar(p);
	} while (*p && Utf8CharBytes(p) == 1 && *p >= 0x21 && *p <= 0x7e);
    } else {
	p = Utf8AdvanceChar(p);
    }
    *end = p - utf8;
    return 1;
}

static inline size_t Utf8CharBytes(const char *utf8) {
    unsigned char b = *utf8;

    if ((b & 0xc0) == 0x00 ||
	(b & 0xc0) == 0x40) {
	return 1;
    } else if ((b & 0xc0) == 0xc0) {
	if (b >> 1 == 126) {
	    return 6;
	} else if (b >> 2 == 62) {
	    return 5;
	} else if (b >> 3 == 30) {
	    return 4;
	} else if (b >> 4 == 14) {
	    return 3;
	} else if (b >> 5 == 6) {
	    return 2;
	}
    }
    abort();
}

// NUL文字でも停止しない。
const char *Utf8AdvanceChar(const char *utf8)
{
    return utf8 + Utf8CharBytes(utf8);
}

size_t Utf8CountChars(const char *utf8)
{
    size_t count;

    count = 0;
    while (*utf8 != '\0') {
	utf8 = Utf8AdvanceChar(utf8);
	count++;
    }

    return count;
}

size_t Utf8CountCharsBuffer(const char *utf8, size_t length)
{
    const char *p = utf8;
    size_t count = 0;

    while (p < utf8 + length) {
	p = Utf8AdvanceChar(p);
	count++;
    }

    return count;
}

bool TokenIsSpace(Token *tok)
{
    return isspace(tok->chars[0].utf8[0]);
}

void CharacterInitialize(Character *ch, short x, const char *utf8, size_t bytes)
{
    ch->utf8 = GC_STRNDUP(utf8, bytes);
    ch->x = x;

    XGlyphInfo extents;
    XftTextExtentsUtf8(disp, font, (FcChar8 *) ch->utf8, bytes, &extents);
    
    ch->width = extents.xOff;
}

short TokenInitialize(Token *tok, short x, const char *utf8, size_t bytes)
{
    tok->x = x;
    tok->nchars = Utf8CountCharsBuffer(utf8, bytes);
    tok->chars = GC_MALLOC(tok->nchars * sizeof(Character));
    const char *p = utf8;
    for (int i = 0; i < tok->nchars; i++) {
	CharacterInitialize(&tok->chars[i],
			    x - tok->x,
			    p,
			    Utf8CharBytes(p));
	x += tok->chars[i].width;
	p = Utf8AdvanceChar(p);
    }
    tok->width = x - tok->x;

    return x;
}

void InspectLine(VisualLine *line)
{
    printf("LINE[");
    for (size_t i = 0; i < line->ntokens; i++) {
	printf("%d, ", (int) line->tokens[i].width);
    }
    printf("]\n");
}


// ぶらさがり空白トークンを除いた、行の終端位置を示す Token ポインタを返す。
// ぶらさがり空白トークンが無い場合は、デリファレンスできないので注意する。
Token *EffectiveLineEnd(VisualLine *line)
{
    size_t index;

    for (index = line->ntokens - 1; index >= 0; index--) {
	if (!TokenIsSpace(&line->tokens[index])) {
	    break;
	}
    }
    return &line->tokens[index + 1];
}

// ragged right でフォーマットされた行を両端揃えにする。
void JustifyLine(VisualLine *line, const PageInfo *page)
{
    Token *trailing_space_start; // 仮想の行末。残りの空白は右マージンに被せる。

    trailing_space_start = EffectiveLineEnd(line);

    // この行に空白しか無い場合は何もしない。
    if (trailing_space_start == line->tokens)
	return;

    int nspaces = 0;
    for (Token *tok = line->tokens; tok < trailing_space_start; tok++)
	if (TokenIsSpace(tok))
	    nspaces++;

    // 空白トークンが無いので、その幅も調整できない。
    if (nspaces == 0)
	return;

    // 最後の空白でないトークン。
    Token *last_token = trailing_space_start - 1;
    short right_edge = last_token->x + last_token->width;
    assert(page->margin_right >= right_edge);

    // それぞれの空白トークンについて、増やすべき幅を計算する。
    int *addends = alloca(nspaces * sizeof(int));
    short shortage = page->margin_right - right_edge;
    Distribute(shortage, nspaces, addends);

    // ぶらさがっていない空白トークンに幅を分配する。
    int i = 0;
    int SPACE_STRETCH_LIMIT = WordWidth(font, " ", 1) * 4;
    for (Token *tok = line->tokens; tok != trailing_space_start; tok++) {
	if (TokenIsSpace(tok)) {
	    int addend = (addends[i] > SPACE_STRETCH_LIMIT) ? SPACE_STRETCH_LIMIT : addends[i];

	    tok->width += addend;
	    i++;
	}
    }

    // 更新された幅を元にトークンの x 座標を再計算する。
    short x = page->margin_left;
    for (Token *tok = line->tokens; tok < line->tokens + line->ntokens; tok++) {
	tok->x = x;
	x += tok->width;
    }
}

VisualLine *CreateLines(const char *text, const PageInfo *page, size_t *lines_return)
{
    VisualLine *lines = GC_MALLOC(MAX_LINES * sizeof(VisualLine));
    size_t nlines = 0;

    size_t start = 0;
    size_t next;
    bool more_tokens;

    do {
	nlines++;
	if (nlines == MAX_LINES) {
	    fprintf(stderr, "too many lines\n");
	    exit(1);
	}

	short x = page->margin_left;
	Token *tokens = NULL;
	size_t ntokens = 0;
	while ((more_tokens = NextTokenBilingual(text, start, &next)) == true) {
	    ntokens++;
	    tokens = GC_REALLOC(tokens, ntokens * sizeof(Token));
	    size_t len = next - start;
	    x = TokenInitialize(&tokens[ntokens-1],
				x,
				&text[start],
				len);
	    if (x > page->margin_right && ntokens > 1 && !TokenIsSpace(&tokens[ntokens-1])) {
		// このトークンの追加をキャンセルする。
		ntokens--;
		tokens = GC_REALLOC(tokens, ntokens * sizeof(Token));
		break;
	    }
	    start = next;
	    if (tokens[ntokens-1].chars[0].utf8[0] == '\n')
		break;
	}

	// 行の完成。
	lines[nlines-1].tokens = tokens;
	lines[nlines-1].ntokens = ntokens;

	if (more_tokens && tokens[ntokens-1].chars[0].utf8[0] != '\n')
	    JustifyLine(&lines[nlines-1], page);
    } while (more_tokens);

    *lines_return = nlines;
    return lines;
}

Document *CreateDocument(const char *text, PageInfo *page)
{
    size_t nlines;

    Document *doc = GC_MALLOC(sizeof(Document));
    doc->lines = CreateLines(text, page, &nlines);
    doc->nlines = nlines;
    doc->page = page;

    doc->max_offset = Utf8CountChars(text);

    return doc;
}

int LeadingAboveLine(XftFont *font);
int LeadingBelowLine(XftFont *font);

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

// str から始まる bytes バイトの文字列の幅を算出する。
int WordWidth(XftFont *font, const char *str, int bytes)
{
    XGlyphInfo extents;

    XftTextExtentsUtf8(disp, font, (FcChar8 *) str, bytes, &extents);
    return extents.xOff;
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

CursorPath ToCursorPath(Document *doc, size_t offset)
{
    size_t count = 0;

    for (int i = 0; i < doc->nlines; i++) {
	for (int j = 0; j < doc->lines[i].ntokens; j++) {
	    for (int k = 0; k < doc->lines[i].tokens[j].nchars; k++) {
		if (count == offset) {
		    return (CursorPath) { .line = i, .token = j, .character = k };
		}
		count++;
	    }
	}
    }
    fprintf(stderr, "ToCursorPath: out of range\n");
    abort();
}

#define DRAW_BASELINE 1
#define DRAW_LEADING 0
#define DRAW_SPACE 1
#define DRAW_RETURN 1
#define DRAW_MARGINS 0

void DrawCursor(XftDraw *draw, short x, short y)
{
#if 0
    // 文字の高さのカーソル。
    XftDrawRect(draw, ColorGetXftColor("magenta"),
		x - 1, y - font->ascent,
		2, font->ascent + font->descent);
#else
    // 行の高さのカーソル。
    XftDrawRect(draw, ColorGetXftColor("magenta"),
		x - 1, y - font->ascent - LeadingAboveLine(font),
		2, font->height);
#endif
};

void DrawLeadingAboveLine(XftDraw *draw, PageInfo *page, short y)
{
    // 上の行間を描画する。
    if (DRAW_LEADING)
	XftDrawRect(draw, ColorGetXftColor("navajo white"),
		    page->margin_left, y - font->ascent - LeadingAboveLine(font),
		    page->margin_right - page->margin_left, LeadingAboveLine(font));
}

void DrawLeadingBelowLine(XftDraw *draw, PageInfo *page, short y)
{
    // 下の行間を描画する。
    if (DRAW_LEADING)
	XftDrawRect(draw, ColorGetXftColor("cornflower blue"),
		    page->margin_left, y + font->descent,
		    page->margin_right - page->margin_left, LeadingBelowLine(font));
}

void DrawBaseline(XftDraw *draw, PageInfo *page, short y)
{
    // ベースラインを描画する。
    if (DRAW_BASELINE)
	XftDrawRect(draw, ColorGetXftColor("gray90"),
		    page->margin_left, y,
		    page->margin_right - page->margin_left, 1);
}

#define NEWLINE_SYMBOL "↓"
void DrawNewline(XftDraw *draw, short x, short y)
{
    if (DRAW_RETURN)
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("cyan4"),
			  font,
			  x, y,
			  (FcChar8 *) NEWLINE_SYMBOL, sizeof(NEWLINE_SYMBOL) - 1);
}

void DrawSpace(XftDraw *draw, short x, short y, short width)
{
    if (DRAW_SPACE)
	XftDrawRect(draw, ColorGetXftColor("misty rose"),
		    x, y - font->ascent,
		    width, font->ascent + font->descent);
}

void DrawPrintableToken(XftDraw *draw, Token *tok, short y)
{
    for (int i = 0; i < tok->nchars; i++) {
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("black"),
			  font,
			  tok->x + tok->chars[i].x, y,
			  (FcChar8 *) tok->chars[i].utf8,
			  strlen(tok->chars[i].utf8));
    }
}

void DrawToken(XftDraw *draw, Token *tok, short y)
{
    if (TokenIsSpace(tok)) {
	switch (tok->chars[0].utf8[0]) {
	case ' ':
	    DrawSpace(draw, tok->x, y, tok->width);
	    break;
	case '\n':
	    DrawNewline(draw, tok->x, y);
	    break;
	}	
    } else {
	// 普通の文字からなるトークン
	DrawPrintableToken(draw, tok, y);
    }
}

// 行を描画する前に実行する。
void DrawLineBefore(XftDraw *draw, PageInfo *page, short y)
{
    DrawLeadingAboveLine(draw, page, y);
    DrawLeadingBelowLine(draw, page, y);
    DrawBaseline(draw, page, y);
}

void DrawLine(XftDraw *draw, PageInfo *page, VisualLine *lines, size_t index, short y)
{
    VisualLine *line = &lines[index];

    DrawLineBefore(draw, page, y);

    // 行の描画
    for (int i = 0; i < line->ntokens; i++) {
	Token *tok = &line->tokens[i];

	DrawToken(draw, tok, y);

	// カーソルを描画する。
	for (int j = 0; j < tok->nchars; j++) {
	    if (CursorPathEquals(cursor_path, (CursorPath) { index, i, j }))
		DrawCursor(draw, tok->x + tok->chars[j].x, y);
	}
    }
}

size_t DrawDocument(XftDraw *draw, Document *doc, size_t start)
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

void MarkMargins(PageInfo *page)
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
    printf("%d\n", (int) cursor_offset);
    printf("%d.%d.%d\n",
	   (int) cursor_path.line,
	   (int) cursor_path.token,
	   (int) cursor_path.character);

    if (doc->page->width < doc->page->margin_left * 2) {
	fprintf(stderr, "Viewport size too small.\n");
	return;
    }

    XftDraw *draw = XftDrawCreate(disp, back_buffer,
				  DefaultVisual(disp,DefaultScreen(disp)), DefaultColormap(disp,DefaultScreen(disp)));

 Retry:
    XftDrawRect(draw, ColorGetXftColor("white"), 0, 0, doc->page->width, doc->page->height);

    if (DRAW_MARGINS)
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
    XdbeSwapInfo swap_info = {
	.swap_window = win,
	.swap_action = XdbeUndefined,
    };
    XdbeSwapBuffers(disp, &swap_info, 1);
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

char *ReadFile(const char *filepath)
{
    FILE *file;

    file = fopen(filepath, "r");
    if (file == NULL) {
	perror("fopen");
	exit(1);
    }

    fseek(file, 0, SEEK_END);

    long length;
    length = ftell(file);

    rewind(file);

    char *buf = GC_MALLOC(length);

    size_t bytes_read;
    bytes_read = fread(buf, 1, length, file);

    if (bytes_read != length) {
	fprintf(stderr, "file size mismatch");
	exit(1);
    }

    fclose(file);

    return buf;
}

void HandleKeyPress(XKeyEvent *ev)
{
    bool needs_redraw = false;

    KeySym sym;
    sym = XLookupKeysym(ev, 0);

    switch (sym) {
    case XK_Right:
	if (cursor_offset != doc->max_offset) {
	    cursor_offset++;
	    cursor_path = ToCursorPath(doc, cursor_offset);
	    needs_redraw = true;
	}
	break;
    case XK_Left:
	if (cursor_offset != 0) {
	    cursor_offset--;
	    cursor_path = ToCursorPath(doc, cursor_offset);
	    needs_redraw = true;
	}
	break;
    case XK_Up:
	needs_redraw = true;
	break;
    case XK_Down:
	needs_redraw = true;
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
    doc = CreateDocument(text, &page);
    cursor_path = ToCursorPath(doc, cursor_offset);

    XEvent ev;
    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	switch (ev.type) {
	case Expose:
	    puts("expose");
	    GetPageInfo(&page);
	    doc = CreateDocument(text, &page);
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
