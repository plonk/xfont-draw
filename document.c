#include <stdio.h>
#include <string.h>
#include <gc.h>
#include <stdlib.h>
#include <ctype.h>
#include <alloca.h>
#include <assert.h>

#include "util.h"
#include "xfont-editor-xft.h"
#include "xfont-editor-xft-utf8.h"
#include "document.h"
#include "hash.h"

extern XftFont *font;

// EOF番兵文字のプロトタイプ
static const Character EOF_CHARACTER =  {
    .x = 0,
    .width = 0,
    .utf8 = "",
    .length = 0
};

extern int TextWidthUncached(XftFont *font, const char *str, int bytes);

// str から始まる bytes バイトの文字列の幅を算出する。
int TextWidth(XftFont *aFont, const char *str, int bytes)
{
    static Hash *cache = NULL;

    if (cache == NULL)
	cache = HashCreateN(4096);

    // デフォルトフォントでない場合はキャッシュしない。
    if (aFont != font)
	return TextWidthUncached(aFont, str, bytes);

    String key = { str, bytes };

    int *pWidth = HashGet(cache, key);

    if (pWidth == NULL) {
	pWidth = GC_MALLOC(sizeof(int));
	*pWidth = TextWidthUncached(aFont, str, bytes);

	HashSet(cache, key, pWidth);
    }
    return *pWidth;
}

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
    assert(bytes <= MAX_UTF8_CHAR_LENGTH);
    strncpy(ch->utf8, utf8, bytes);
    ch->utf8[bytes] = '\0';
    ch->length = bytes;
    ch->x = x;
    ch->width = TextWidth(font, ch->utf8, bytes);
}

Token *TokenCreate()
{
    return GC_MALLOC(sizeof(Token));
}

void TokenInitialize(Token *tok)
{
    memset(tok, 0, sizeof(*tok));
}

short TokenEOF(Token *tok, short x)
{
    tok->x = x;
    tok->nchars = 1;
    tok->chars = GC_MALLOC(sizeof(Character));

    CharacterInitialize(tok->chars, x - tok->x, "", 0);
    return x;
}

bool CharacterIsEOF(Character *ch)
{
    return ch->length == 0;
}

bool TokenIsEOF(Token *tok)
{
    return tok->nchars == 1 && CharacterIsEOF(tok->chars);
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
    off_t index;

    for (index = line->ntokens - 1; index >= 0; index--) {
	if (!TokenIsSpace(&line->tokens[index])) {
	    break;
	}
    }
    return &line->tokens[index + 1];
}

// トークンの幅を元にトークンの x 座標を再計算する。
void UpdateTokenPositions(VisualLine* line)
{
    short x = 0;
    for (Token *tok = line->tokens; tok < line->tokens + line->ntokens; tok++) {
	tok->x = x;
	x += tok->width;
    }
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
	if (tok->chars[0].utf8[0] == ' ')
	    nspaces++;

    // 空白トークンが無いので、その幅も調整できない。
    if (nspaces == 0)
	goto Tracking;

    // 最後の空白でないトークン。
    Token *last_token = trailing_space_start - 1;
    short right_edge = last_token->x + last_token->width;
    // printf("%hd vs %hd\n", PageInfoGetVisibleWidth(page), right_edge);
    // assert(PageInfoGetVisibleWidth(page) >= right_edge);
    if (PageInfoGetVisibleWidth(page) <= right_edge)
	goto Tracking;

    // それぞれの空白トークンについて、増やすべき幅を計算する。
    int *addends = alloca(nspaces * sizeof(int));
    short shortage = PageInfoGetVisibleWidth(page) - right_edge;
    Distribute(shortage, nspaces, addends);

    // ぶらさがっていない空白トークンに幅を分配する。
    int i = 0;
    int SPACE_STRETCH_LIMIT = TextWidth(font, " ", 1) * 3;
    for (Token *tok = line->tokens; tok != trailing_space_start; tok++) {
	if (TokenIsSpace(tok)) {
	    int addend = (addends[i] > SPACE_STRETCH_LIMIT) ? SPACE_STRETCH_LIMIT : addends[i];

	    tok->width += addend;
	    i++;
	}
    }

    UpdateTokenPositions(line);

 Tracking:
    last_token = trailing_space_start - 1;
    right_edge = last_token->x + last_token->width;
    shortage = PageInfoGetVisibleWidth(page) - right_edge;
    size_t ntokens = trailing_space_start - line->tokens - 1;

    if (ntokens == 0) {
	// 最初のトークンが最後の可視トークンである場合。
	return;
    }

    short MAX_TRACK_DELTA = (short) (EmPixels(font) / 8.0);

    if (shortage == 0) {
	return;
    } if (shortage > 0) {
	int addends[ntokens];
	Distribute(shortage, ntokens, addends);

	for (int i = 0; i < ntokens; i++) {
	    line->tokens[i].width += addends[i] > MAX_TRACK_DELTA ? MAX_TRACK_DELTA : addends[i];
	}
    } else {
	short excess = -shortage;
	int subtrahends[ntokens];
	Distribute(excess, ntokens, subtrahends);

	for (int i = 0; i < ntokens; i++) {
	    line->tokens[i].width -= subtrahends[i] > MAX_TRACK_DELTA ? MAX_TRACK_DELTA : subtrahends[i];
	}
    }
    UpdateTokenPositions(line);
}

// 文字列を Character レコードの配列に変換する。
// 個々の Character の x 座標は 0 に設定される。
Character *StringToCharacters(const char *text, size_t length, size_t *nchars_return)
{
    const char *p;
    // 上限値で確保する。
    Character *ret = GC_MALLOC(sizeof(Character) * (length + 1));
    Character *q = ret;

    printf("StringToCharacters... %d bytes\n", (int) length);
    for (p = text; p < text + length; p = Utf8AdvanceChar(p)) {
	CharacterInitialize(q++, 0, p, Utf8CharBytes(p));
    }
    *q++ = EOF_CHARACTER;
    *nchars_return = q - ret;
    // 無駄な部分を解放する。
    ret = GC_REALLOC(ret, sizeof(Character) * (q - ret));
    puts("Done");

    return ret;
}

void TokenAddCharacter(Token *tok, Character *ch)
{
    assert (ch != NULL);

    tok->chars = GC_REALLOC(tok->chars, (tok->nchars + 1) * sizeof(Character));
    tok->chars[tok->nchars] = *ch;

    tok->chars[tok->nchars].x = tok->width;
    tok->width += ch->width;
    tok->nchars++;
}
Character *Tokenize(Character *ch, Token *tok)
{
    assert( ch != NULL );
    TokenInitialize(tok);

    while (IsForbiddenAtEnd(ch->utf8)) {
	TokenAddCharacter(tok, ch);
	ch++;
    }

    if (streq(ch->utf8, " ")) {
	do {
	    TokenAddCharacter(tok, ch);
	    ch++;
	} while (streq(ch->utf8, " "));
	goto End;
    } else if (CTypeOf(ch->utf8, isalnum) ||
	       streq(ch->utf8, "\'") ||
	       streq(ch->utf8, ".") ||
	       streq(ch->utf8, ",")) {
	do {
	    TokenAddCharacter(tok, ch);
	    ch++;
	} while (CTypeOf(ch->utf8, isalnum) ||
		 streq(ch->utf8, "\'") ||
		 streq(ch->utf8, ".") ||
		 streq(ch->utf8, ","));
    } else {
	if (CharacterIsEOF(ch) || streq(ch->utf8, "\n")) {
	    TokenAddCharacter(tok, ch);
	    ch++;
	    goto End;
	} else {
	    TokenAddCharacter(tok, ch);
	    ch++;
	}
    }

    // 行頭禁止文字が続いていたら連結する。
    while (IsForbiddenAtStart(ch->utf8)) {
	TokenAddCharacter(tok, ch);
	ch++;
    }

 End:
    return ch;
}

Token *CharactersToTokens(Character text[], size_t nchars, size_t *ntokens_return)
{
    assert(text != NULL);

    puts("CharactersToTokens...");
    // nchars がトークン数の上限である。
    Token *res = GC_MALLOC(sizeof(Token) * nchars);
    size_t ntokens = 0;
    Character *p = text;

    while (p < text + nchars) {
	p = Tokenize(p, &res[ntokens++]);
    }
    puts("Done");
    res = GC_REALLOC(res, sizeof(Token) * ntokens);

    *ntokens_return = ntokens;
    return res;
}

void VisualLineAddToken(VisualLine *line, Token *tok)
{
    assert (tok != NULL);

    line->tokens = GC_REALLOC(line->tokens, (line->ntokens + 1) * sizeof(Token));
    line->tokens[line->ntokens] = *tok;

    short x = 0;
    if (line->ntokens > 0) {
	x = line->tokens[line->ntokens - 1].x + line->tokens[line->ntokens - 1].width;
    }
    line->tokens[line->ntokens].x = x;
    if (line->tokens[line->ntokens].chars[0].utf8[0] == '\t') {
	short tab_width = TextWidth(font, " ", 1) * 8; 
	line->tokens[line->ntokens].width = (x / tab_width + 1) * tab_width - x;
    }
    line->ntokens++;
}

short PageInfoGetVisibleWidth(const PageInfo *page)
{
    return page->margin_right - page->margin_left;
}

VisualLine *VisualLineCreate()
{
    return GC_MALLOC(sizeof(VisualLine));
}

short VisualLineGetWidth(VisualLine *line)
{
    if (line->ntokens == 0)
	return 0;
    else
	return line->tokens[line->ntokens - 1].x + line->tokens[line->ntokens - 1].width;
}

double EmPixels(XftFont *font)
{
    double em;

    FcPatternGetDouble(font->pattern, FC_PIXEL_SIZE, 0, &em);
    return em;
}

Token *FillLine(VisualLine **line_return, Token *input, const PageInfo *page)
{
    assert (input != NULL);

    short visible_width = PageInfoGetVisibleWidth(page);
    VisualLine *line = VisualLineCreate();

    while (1) {
	if (!(line->ntokens == 0 || TokenIsSpace(input))) {
	    bool line_is_full = VisualLineGetWidth(line) + input->width > visible_width + (short) (EmPixels(font) * 0.75);
	    if (line_is_full) {
		// このトークンの追加をキャンセルする。
		break;
	    }
	}
	VisualLineAddToken(line, input);
	if (TokenIsEOF(input) || TokenIsNewline(input)) {
	    input++;
	    break;
	} else {
	    input++;
	}
    } 

    // 行の完成。
    *line_return = line;
    return input;
}

bool LastLineOfParagraph(VisualLine *line)
{
    assert(line->ntokens > 0);
    Token *last_token = &line->tokens[line->ntokens-1];
    
    return TokenIsNewline(last_token) || TokenIsEOF(last_token);
}

void InspectPageInfo(const PageInfo *page)
{
    printf("#<%p:PageInfo ", page);
    printf("width=%hd, ", page->width);
    printf("height=%hd, ", page->height);
    printf("margin_top=%hd, ", page->margin_top);
    printf("margin_right=%hd, ", page->margin_right);
    printf("margin_bottom=%hd, ", page->margin_bottom);
    printf("margin_left=%hd", page->margin_left);
    printf(">\n");
}

VisualLine *CreateLines(Token *tokens, size_t ntokens, const PageInfo *page, size_t *nlines_return)
{
    VisualLine *lines = NULL;
    size_t nlines = 0;
    Token *tok = tokens;

    // InspectPageInfo(page);
    VisualLine *line;
    do {
	lines = GC_REALLOC(lines, (nlines + 1) * sizeof(VisualLine));
	tok = FillLine(&line, tok, page);

 	if (!LastLineOfParagraph(line))
	    JustifyLine(line, page);

	lines[nlines] = *line;
	nlines++;
    } while (tok < tokens + ntokens);

    *nlines_return = nlines;
    return lines;
}

void TokenResetWidth(Token *tok)
{
    assert(tok->nchars > 0);
    tok->width = tok->chars[tok->nchars - 1].x + tok->chars[tok->nchars - 1].width;
}

Token *ExtractTokens(Document *doc, size_t *ntokens_return)
{
    size_t ntokens = 0;
    for (int i = 0; i < doc->nlines; i++) {
	ntokens += doc->lines[i].ntokens;
    }

    Token *tokens = GC_MALLOC(sizeof(Token) * ntokens);

    int k = 0;
    for (int i = 0; i < doc->nlines; i++) {
	for (int j = 0; j < doc->lines[i].ntokens; j++) {
	    tokens[k] = doc->lines[i].tokens[j];
	    TokenResetWidth(&tokens[k]);
	    k++;
	}
    }
    assert(k == ntokens);

    *ntokens_return = ntokens;
    return tokens;
}

void MendDocument(Document *doc)
{
    size_t ntokens;
    Token *tokens = ExtractTokens(doc, &ntokens);

    doc->lines = CreateLines(tokens, ntokens, doc->page, &doc->nlines);
}

Document *CreateDocument(const char *text, const PageInfo *page)
{
    Document *doc = GC_MALLOC(sizeof(Document));

    size_t nchars;
    Character *chars = StringToCharacters(text, strlen(text), &nchars); // TODO: ファイルの長さで作る
    size_t ntokens;
    Token *tokens = CharactersToTokens(chars, nchars, &ntokens);

    doc->lines = CreateLines(tokens, ntokens, page, &doc->nlines);
    doc->page = GC_MALLOC(sizeof(PageInfo));
    *doc->page = *page;

    return doc;
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

size_t CursorPathToCharacterOffset(Document *doc, CursorPath path)
{
    
    size_t count = 0;

    for (CursorPath it = { 0, 0, 0 };
	 !CharacterIsEOF(CursorPathGetCharacter(doc, it)) && !CursorPathEquals(it, path)
	 ; it = CursorPathForward(doc, it)) {
	count++;
    }

    return count;
}

CursorPath CursorPathForward(Document *doc, CursorPath path)
{
    if (CursorPathIsEnd(doc, path)) {
	return path;
    } else {
	Token *tok = GetToken(doc, path.line, path.token);
	VisualLine *line = GetLine(doc, path.line);
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

bool CursorPathIsEnd(Document *doc, CursorPath path)
{
    return TokenIsEOF(GetToken(doc, path.line, path.token));
}

Character *CursorPathGetCharacter(Document *doc, CursorPath path)
{
    return GetCharacter(doc, path.line, path.token, path.character);
}

Character *GetCharacter(Document *doc, size_t line, size_t token, size_t character)
{
    Token *tok = GetToken(doc, line, token);
    assert(character < tok->nchars);
    return &tok->chars[character];
}

Token *GetToken(Document *doc, size_t line, size_t token)
{
    VisualLine *ln = GetLine(doc, line);

    assert(token < ln->ntokens );
    return &ln->tokens[token];
}

VisualLine *GetLine(Document *doc, size_t line)
{
    assert(line < doc->nlines);
    return &doc->lines[line];
}

void DocumentSetPageInfo(Document *doc, PageInfo *page)
{
    *doc->page = *page;
    MendDocument(doc);
}

CursorPath CursorPathBackward(Document *doc, CursorPath path)
{
    if (CursorPathIsBegin(path)) {
	return path;
    } else {
	if (path.character > 0) {
	    return (CursorPath) { path.line, path.token, path.character - 1 };
	} else if (path.token > 0) {
	    Token *tok = GetToken(doc, path.line, path.token - 1);
	    return (CursorPath) { path.line, path.token - 1, tok->nchars - 1 };
	} else {
	    assert(path.line > 0);

	    VisualLine *line = GetLine(doc, path.line - 1);
	    Token *tok = GetToken(doc, path.line - 1, line->ntokens - 1);
	    return (CursorPath) { path.line - 1, line->ntokens - 1, tok->nchars - 1};
	}
    }
}

short CursorPathGetX(Document *doc, CursorPath path)
{
    return GetToken(doc, path.line, path.token)->x + CursorPathGetCharacter(doc, path)->x;
}
