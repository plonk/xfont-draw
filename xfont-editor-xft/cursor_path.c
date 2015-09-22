#include <assert.h>
#include "cursor_path.h"

bool CursorPathEquals(CursorPath a, CursorPath b)
{
    return (a.line == b.line &&
	    a.token == b.token &&
	    a.character == b.character);
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
