#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <X11/Xft/Xft.h>
#include "cursor_path.h"

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



CursorPath ToCursorPath(Document *doc, size_t offset);
Document *CreateDocument(const char *text, const PageInfo *page);
Character *GetCharacter(Document *, size_t line, size_t token, size_t character);
bool TokenIsEOF(Token *tok);
size_t CursorPathToCharacterOffset(Document *doc, CursorPath path);
Character *CursorPathGetCharacter(Document *doc, CursorPath path);
bool CursorPathEquals(CursorPath a, CursorPath b);
short CursorPathGetX(Document *doc, CursorPath path);
VisualLine *GetLine(Document *doc, size_t line);
CursorPath CursorPathForward(Document *doc, CursorPath path);
CursorPath CursorPathBackward(Document *doc, CursorPath path);
bool CursorPathIsBegin(CursorPath path);
bool CursorPathIsEnd(Document *doc, CursorPath path);
double EmPixels(XftFont *font);
void DocumentSetPageInfo(Document *doc, PageInfo *page);
int TextWidth(XftFont *aFont, const char *str, int bytes);
short PageInfoGetVisibleWidth(const PageInfo *page);
Token *GetToken(Document *doc, size_t line, size_t token);
bool CharacterIsEOF(Character *ch);

#endif
