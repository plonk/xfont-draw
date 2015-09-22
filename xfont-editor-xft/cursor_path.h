#ifndef CURSOR_PATH_H
#define CURSOR_PATH_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t line;
    size_t token;
    size_t character;
} CursorPath;

#include "document.h"

CursorPath CursorPathBackward(Document *doc, CursorPath path);
bool CursorPathEquals(CursorPath a, CursorPath b);
CursorPath CursorPathForward(Document *doc, CursorPath path);
Character *CursorPathGetCharacter(Document *doc, CursorPath path);
short CursorPathGetX(Document *doc, CursorPath path);
bool CursorPathIsBegin(CursorPath path);
bool CursorPathIsEnd(Document *doc, CursorPath path);
size_t CursorPathToCharacterOffset(Document *doc, CursorPath path);
Character *GetCharacter(Document *doc, size_t line, size_t token, size_t character);
CursorPath ToCursorPath(Document *doc, size_t offset);

#endif
