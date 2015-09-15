/**
 * Helvetica で英文を表示するプログラム。両端揃え。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "util.h"
#include <X11/Xft/Xft.h>
#include <gc.h>
#include <stdbool.h>
#include <alloca.h>

#include <ctype.h>
#include <assert.h>

#define FONT_DESCRIPTION "Source Han Sans JP-20:matrix=1 0 0 1"

Display *disp;
Window win;
XftFont *font;
XftColor black;

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

#define MAX_LINES 1024

int WordWidth(XftFont *font, const char *str, int len);

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

const char *Utf8AdvanceChar(const char *utf8)
{
    if (*utf8 == '\0') {
	fprintf(stderr, "Utf8AdvanceChar: no char\n");
	abort();
    }
    return utf8 + Utf8CharBytes(utf8);
}

bool TokenIsSpace(Token *tok)
{
    return isspace(tok->chars[0].utf8[0]);
}

void CharacterCreateInPlace(Character *ch, short x, const char *utf8, size_t bytes)
{
    ch->utf8 = GC_STRNDUP(utf8, bytes);
    ch->x = x;

    XGlyphInfo extents;
    XftTextExtentsUtf8(disp, font, (FcChar8 *) ch->utf8, bytes, &extents);
    
    ch->width = extents.xOff;
}

void TokenCreateInPlace(Token *tok, short *x_in_out, const char *utf8, size_t bytes)
{
    short x = *x_in_out;
    tok->x = x;
    tok->chars = NULL;
    tok->nchars = 0;
    for (const char *p = utf8; p - utf8 < bytes; p = Utf8AdvanceChar(p)) {
	tok->nchars++;
	tok->chars = GC_REALLOC(tok->chars, tok->nchars * sizeof(Character));
	CharacterCreateInPlace(tok->chars + tok->nchars - 1,
			       x - tok->x,
			       p,
			       Utf8CharBytes(p));
	x += tok->chars[tok->nchars - 1].width;
    }
    tok->width = x - *x_in_out;
    *x_in_out = x;
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

VisualLine *CreateDocument(const char *text, const PageInfo *page, size_t *lines_return)
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
	while ((more_tokens = NextToken(text, start, &next)) == true) {
	    ntokens++;
	    tokens = GC_REALLOC(tokens, ntokens * sizeof(Token));
	    size_t len = next - start;
	    TokenCreateInPlace(&tokens[ntokens-1],
			       &x,
			       &text[start],
			       len);
	    if (x > page->margin_right && ntokens > 1 && !TokenIsSpace(&tokens[ntokens-1])) {
		// このトークンの追加をキャンセルする。
		ntokens--;
		tokens = GC_REALLOC(tokens, ntokens * sizeof(Token));
		break;
	    }
	    start = next;
	}

	// 行の完成。
	lines[nlines-1].tokens = tokens;
	lines[nlines-1].ntokens = ntokens;

	if (more_tokens)
	    JustifyLine(&lines[nlines-1], page);
    } while (more_tokens);

    *lines_return = nlines;
    return lines;
}

int LeadingAboveLine(XftFont *font);

// 行の上に置くべき行間を算出する。
int LeadingAboveLine(XftFont *font)
{
    int lineSpacing = font->height - (font->ascent + font->descent);

    return lineSpacing / 2;
}

void GetGlyphInfo(char ch, XftFont *font, XGlyphInfo *extents_return)
{
    char str[7] = ""; // 最長のUTF8文字が入る大きさを確保する。

    str[0] = ch;
    XftTextExtentsUtf8(disp, font, (FcChar8 *) str, 1, extents_return);
}

// str から始まる len 文字の幅を算出する。
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

struct Token {
    int width;
    size_t start;
    size_t length;
};

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

void DrawLine(XftDraw *draw, VisualLine *line, short y)
{
    // 行の描画
    for (int i = 0; i < line->ntokens; i++) {
	Token *tok = &line->tokens[i];

	if (TokenIsSpace(tok))
	    continue;

	for (int j = 0; j < tok->nchars; j++) {
	    Character *ch = &tok->chars[j];

	    XftDrawStringUtf8(draw, &black, font,
			      tok->x + ch->x, y,
			      (FcChar8 *) ch->utf8,
			      strlen(ch->utf8));
	}
    }
}

size_t DrawDocument(XftDraw *draw, VisualLine *lines, size_t nlines, PageInfo *page)
{
    size_t start = 0;
    short y = page->margin_top + LeadingAboveLine(font) + font->ascent;

    for (size_t i = start; i < nlines; i++) {
	DrawLine(draw, &lines[i], y);
	y += font->height;

	short next_line_ink_bottom = y + LeadingAboveLine(font) + font->ascent + font->descent;
	if (next_line_ink_bottom > page->margin_bottom)
	    return i;
    }
    return nlines;
}

void Redraw(const char *text)
{
    PageInfo page;
    GetPageInfo(&page);

    if (page.width < page.margin_left * 2) {
	fprintf(stderr, "Viewport size too small.\n");
	return;
    }

    XftDraw *draw = XftDrawCreate(disp, win, DefaultVisual(disp,DefaultScreen(disp)), DefaultColormap(disp,DefaultScreen(disp)));

    XClearWindow(disp, win);

    size_t nlines;
    VisualLine *lines = CreateDocument(text, &page, &nlines);

    DrawDocument(draw, lines, nlines, &page);
}

void Initialize()
{

    disp = XOpenDisplay(NULL); // open $DISPLAY
    win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 640, 480, 0, 0, WhitePixel(disp, DefaultScreen(disp)));	
    XMapWindow(disp, win);
    // 暴露イベントを受け取る。
    XSelectInput(disp, win, ExposureMask);

    font = XftFontOpenName(disp, DefaultScreen(disp), FONT_DESCRIPTION);

    // 「黒」を割り当てる。
    XftColorAllocName(disp,
		      DefaultVisual(disp,DefaultScreen(disp)),
		      DefaultColormap(disp,DefaultScreen(disp)),
		      "black", &black);
}

void CleanUp(Display *disp, Window win, XftFont *font)
{
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}

int main()
{

    Initialize();

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

	Redraw(msg);
    }

    CleanUp(disp, win, font);
}
