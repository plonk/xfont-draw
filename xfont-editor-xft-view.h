#ifndef XFONT_EDITOR_XFT_VIEW_H
#define XFONT_EDITOR_XFT_VIEW_H

#include <stdbool.h>
#include "xfont-editor-xft.h"

void ViewSetOption(const char *name, bool b);
void Redraw();
Document *CreateDocument(const char *text, const PageInfo *page);
CursorPath ToCursorPath(Document *doc, size_t offset);
void ViewInitialize(Display *aDisp, Window aWin, const char *aText, PageInfo *page);
bool ViewForwardCursor();
bool ViewBackwardCursor();
bool ViewUpwardCursor();
bool ViewDownwardCursor();

char *InspectString(const char *str);
void ViewSetPageInfo(PageInfo *page);

#endif
