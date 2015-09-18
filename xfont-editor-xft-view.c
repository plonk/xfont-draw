#include <stdbool.h>
#include <ctype.h>
#include <gc.h>
#include <assert.h>
#include <alloca.h>

#include "util.h"
#include "color.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdbe.h>

#include "xfont-editor-xft.h"
#include "xfont-editor-xft-utf8.h"
#include "xfont-editor-xft-view.h"

static Display *disp;
static Window win;
static XdbeBackBuffer	 back_buffer;
static XftFont *font;

static char *text;
static Document *doc;

static size_t cursor_offset;
static CursorPath cursor_path;
static size_t top_line;

#define MAX_LINES 1024

int WordWidth(XftFont *font, const char *str, int len);

bool CursorPathEquals(CursorPath a, CursorPath b)
{
    return (a.line == b.line &&
	    a.token == b.token &&
	    a.character == b.character);
}

bool TokenIsSpace(Token *tok)
{
    return isspace(tok->chars[0].utf8[0]);
}

bool TokenIsNewline(Token *tok)
{
    return tok->chars[0].utf8[0] == '\n';
}

void CharacterInitialize(Character *ch, short x, const char *utf8, size_t bytes)
{
    ch->utf8 = GC_STRNDUP(utf8, bytes);
    ch->length = bytes;
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

short TokenEOF(Token *tok, short x)
{
    tok->x = x;
    tok->nchars = 1;
    tok->chars = GC_MALLOC(sizeof(Character));

    CharacterInitialize(tok->chars, x - tok->x, "", 0);
    return x;
}

bool TokenIsEOF(Token *tok)
{
    return tok->nchars == 1 && tok->chars[0].length == 0;
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


bool FillLine(VisualLine *line, const char *text, const PageInfo *page, size_t *start_in_out)
{
    short x = page->margin_left;
    Token *tokens = NULL;
    size_t ntokens = 0;
    size_t start = *start_in_out;
    size_t next;
    bool more_tokens;

    while ((more_tokens = NextTokenBilingual(text, start, &next)) == true) {
	tokens = GC_REALLOC(tokens, (ntokens + 1) * sizeof(Token));
	size_t len = next - start;
	x = TokenInitialize(&tokens[ntokens], x, &text[start], len);
	bool line_is_full = x > page->margin_right && !(ntokens == 0 || TokenIsSpace(&tokens[ntokens]));
	if (line_is_full) {
	    // このトークンの追加をキャンセルする。
	    tokens = GC_REALLOC(tokens, ntokens * sizeof(Token));
	    break;
	} else {
	    start = next;
	    if (TokenIsNewline(&tokens[ntokens])) {
		ntokens++;
		break;
	    } else {
		ntokens++;
	    }
	}
    }

    if (!more_tokens) {
	tokens = GC_REALLOC(tokens, (ntokens + 1) * sizeof(Token));
	TokenEOF(&tokens[ntokens], x);
	ntokens++;
    }

    // 行の完成。
    line->tokens = tokens;
    line->ntokens = ntokens;
    *start_in_out = start;

    return more_tokens;
}

bool LastLineOfParagraph(VisualLine *line)
{
    return TokenIsNewline(&line->tokens[line->ntokens-1]);
}

VisualLine *CreateLines(const char *text, const PageInfo *page, size_t *lines_return)
{
    VisualLine *lines = GC_MALLOC(MAX_LINES * sizeof(VisualLine));
    size_t nlines = 0;

    size_t start = 0;
    bool more_tokens;

    do {
	assert( nlines <= MAX_LINES );
	more_tokens = FillLine(&lines[nlines], text, page, &start);

	if (more_tokens && !LastLineOfParagraph(&lines[nlines]))
	    JustifyLine(&lines[nlines], page);

	nlines++;
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
#define DRAW_NEWLINE 1
#define DRAW_MARGINS 0
#define MARK_TOKENS 1

static void DrawCursor(XftDraw *draw, short x, short y)
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
#if 0
    // ベースラインを描画する。
    if (DRAW_BASELINE)
	XftDrawRect(draw, ColorGetXftColor("gray90"),
		    page->margin_left, y,
		    page->margin_right - page->margin_left, 1);
#else
    // 下線。
    if (DRAW_BASELINE)
	XftDrawRect(draw, ColorGetXftColor("gray90"),
		    page->margin_left, y + font->descent + LeadingBelowLine(font),
		    page->margin_right - page->margin_left, 1);
#endif
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

static void DrawPrintableToken(XftDraw *draw, Token *tok, short y)
{
    for (int i = 0; i < tok->nchars; i++) {
	XftDrawStringUtf8(draw,
			  ColorGetXftColor("black"),
			  font,
			  tok->x + tok->chars[i].x, y,
			  (FcChar8 *) tok->chars[i].utf8,
			  strlen(tok->chars[i].utf8));
	if (MARK_TOKENS)
	    // トークン区切りをあらわす下線を引く。
	    XftDrawRect(draw, ColorGetXftColor("green4"),
			tok->x + 2, y + font->descent + LeadingBelowLine(font),
			tok->width - 4, 2);
    }
}

#define EOF_SYMBOL "[EOF]"

static void DrawEOF(XftDraw *draw, short x, short y)
{
    XftDrawStringUtf8(draw,
		      ColorGetXftColor("cyan4"),
		      font,
		      x, y,
		      (FcChar8 *) EOF_SYMBOL,
		      sizeof(EOF_SYMBOL) - 1);
}

static void DrawToken(XftDraw *draw, Token *tok, short y)
{
    if (TokenIsEOF(tok)) {
	DrawEOF(draw, tok->x, y);
    } else {
	switch (tok->chars[0].utf8[0]) {
	case ' ':
	    DrawSpace(draw, tok->x, y, tok->width);
	    break;
	case '\n':
	    DrawNewline(draw, tok->x, y);
	    break;
	default:
	    // 普通の文字からなるトークン
	    DrawPrintableToken(draw, tok, y);
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

	DrawToken(draw, tok, y);

	// カーソルを描画する。
	for (int j = 0; j < tok->nchars; j++) {
	    if (CursorPathEquals(cursor_path, (CursorPath) { index, i, j }))
		DrawCursor(draw, tok->x + tok->chars[j].x, y);
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

#define FONT_DESCRIPTION "Source Han Sans JP-16:matrix=1 0 0 1"

void ViewInitialize(Display *aDisp, Window aWin, 
		    const char *aText, PageInfo *page)
{
    disp = aDisp;
    win = aWin;
    ColorInitialize(disp);
    InitializeBackBuffer();
    font = XftFontOpenName(disp, DefaultScreen(disp), FONT_DESCRIPTION);

    text = GC_STRDUP(aText);
    cursor_offset = 0;
    SetPageInfo(page);
}

void SetPageInfo(PageInfo *page)
{
    doc = CreateDocument(text, page);
    cursor_path = ToCursorPath(doc, cursor_offset);
}

CursorPath CursorPathForward(CursorPath path);
CursorPath CursorPathBackward(CursorPath path);
bool CursorPathIsBegin(CursorPath path);
bool CursorPathIsEnd(CursorPath path);
Token *GetToken(size_t line, size_t token);
VisualLine *GetLine(size_t line);

CursorPath CursorPathForward(CursorPath path)
{
    if (CursorPathIsEnd(path)) {
	return path;
    } else {
	Token *tok = GetToken(path.line, path.token);
	VisualLine *line = GetLine(path.line);
	if (path.character < tok->nchars - 1) {
	    return (CursorPath) { path.line, path.token, path.character + 1 };
	} else if (path.token < line->ntokens - 1) {
	    return (CursorPath) { path.line, path.token + 1, 0 };
	} else {
	    // 非最終行の行末に居る。

	    return (CursorPath) { path.line + 1, 0, 0 };
	}
    }
}

bool CursorPathIsBegin(CursorPath path)
{
    return path.line == 0 && path.token == 0 && path.character == 0;
}

bool CursorPathIsEnd(CursorPath path)
{
    return TokenIsEOF(GetToken(path.line, path.token));
}

Character *GetCharacter(size_t line, size_t token, size_t character);

Character *CursorPathGetCharacter(CursorPath path)
{
    return GetCharacter(path.line, path.token, path.character);
}

Character *GetCharacter(size_t line, size_t token, size_t character)
{
    Token *tok = GetToken(line, token);
    assert(character < tok->nchars);
    return &tok->chars[character];
}

Token *GetToken(size_t line, size_t token)
{
    VisualLine *ln = GetLine(line);

    assert(token < ln->ntokens );
    return &ln->tokens[token];
}

VisualLine *GetLine(size_t line)
{
    assert(line < doc->nlines);
    return &doc->lines[line];
}

CursorPath CursorPathBackward(CursorPath path)
{
    if (CursorPathIsBegin(path)) {
	return path;
    } else {
	if (path.character > 0) {
	    return (CursorPath) { path.line, path.token, path.character - 1 };
	} else if (path.token > 0) {
	    Token *tok = GetToken(path.line, path.token - 1);
	    return (CursorPath) { path.line, path.token - 1, tok->nchars - 1 };
	} else {
	    assert(path.line > 0);

	    VisualLine *line = GetLine(path.line - 1);
	    Token *tok = GetToken(path.line - 1, line->ntokens - 1);
	    return (CursorPath) { path.line - 1, line->ntokens - 1, tok->nchars - 1};
	}
    }
}

bool ViewForwardCursor()
{
    CursorPath newLoc = CursorPathForward(cursor_path);

    if (CursorPathEquals(newLoc, cursor_path))
	return false;
    else {
	cursor_path = newLoc;
	return true;
    }
}

bool ViewBackwardCursor()
{
    CursorPath newLoc = CursorPathBackward(cursor_path);

    if (CursorPathEquals(newLoc, cursor_path))
	return false;
    else {
	cursor_path = newLoc;
	return true;
    }
}

short CursorPathGetX(CursorPath path)
{
    return GetToken(path.line, path.token)->x + CursorPathGetCharacter(path)->x;
}

bool ViewUpwardCursor()
{
    if (cursor_path.line == 0)
	return false;

    short preferred_x = CursorPathGetX(cursor_path);
    CursorPath it = { cursor_path.line, 0, 0 }; // 行頭へ移動する。

    short x;
    do {
	it = CursorPathBackward(it);
	x = CursorPathGetX(it);
    } while (x > preferred_x);

    cursor_path = it;

    return true;
}

bool ViewDownwardCursor()
{
    if (cursor_path.line == doc->nlines - 1)
	return false;

    short preferred_x = CursorPathGetX(cursor_path);
    CursorPath it = { cursor_path.line + 1, 0, 0 }; // 次の行の行頭へ移動する。
    CursorPath target;

    do {
	target = it;
	if (CursorPathGetCharacter(it)->length == 0)
	    break;
	it = CursorPathForward(it);
    } while (it.line == cursor_path.line + 1 && CursorPathGetX(it) <= preferred_x);

    cursor_path = target;

    return true;
}
