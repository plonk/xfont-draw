#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <X11/Xft/Xft.h>

#define MAX_UTF8_CHAR_LENGTH 6

typedef struct {
    short x;
    short width;
    char utf8[MAX_UTF8_CHAR_LENGTH + 1];
    size_t length;
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
} Document;

#include "cursor_path.h"

Character **ExtractCharacters(Document *doc, size_t *nchars_return);
void CharacterInitialize(Character *ch, short x, const char *utf8, size_t bytes);
bool CharacterIsEOF(Character *ch);
Token *CharactersToTokens(Character text[], size_t nchars, size_t *ntokens_return);
Document *CreateDocument(const char *text, const PageInfo *page);
void DocumentSetPageInfo(Document *doc, PageInfo *page);
Token *ExtractTokens(Document *doc, size_t *ntokens_return);
Token *FillLine(VisualLine **line_return, Token *input, const PageInfo *page);
VisualLine *GetLine(Document *doc, size_t line);
Token *GetToken(Document *doc, size_t line, size_t token);
void InspectLine(VisualLine *line);
void JustifyLine(VisualLine *line, const PageInfo *page);
short PageInfoGetVisibleWidth(const PageInfo *page);
void ResetCharacterWidth(Character *ch);
Character *StringToCharacters(const char *text, size_t length, size_t *nchars_return);
void TokenAddCharacter(Token *tok, Character *ch);
Token *TokenCreate(void);
short TokenEOF(Token *tok, short x);
void TokenInitialize(Token *tok);
bool TokenIsEOF(Token *tok);
bool TokenIsNewline(Token *tok);
bool TokenIsSpace(Token *tok);
void UpdateTokenPositions(VisualLine *line);
void VisualLineAddToken(VisualLine *line, Token *tok);
VisualLine *VisualLineCreate(void);
short VisualLineGetWidth(VisualLine *line);

#endif
