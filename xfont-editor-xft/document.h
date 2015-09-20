#ifndef DOCUMENT_H
#define DOCOUMENT_H

#include <stdbool.h>

#include <X11/Xft/Xft.h>

Character *GetCharacter(Document *, size_t line, size_t token, size_t character);
int LeadingAboveLine(XftFont *font);
int LeadingBelowLine(XftFont *font);
bool TokenIsEOF(Token *tok);
size_t CursorPathToCharacterOffset(Document *doc, CursorPath path);
Character *CursorPathGetCharacter(Document *doc, CursorPath path);
bool CursorPathEquals(CursorPath a, CursorPath b);
short CursorPathGetX(Document *doc, CursorPath path);
void MendDocument(Document *doc);
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

#endif
