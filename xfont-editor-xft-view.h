#ifndef XFONT_EDITOR_XFT_VIEW_H
#define XFONT_EDITOR_XFT_VIEW_H

void Redraw();
void GetPageInfo(PageInfo *page);
Document *CreateDocument(const char *text, PageInfo *page);
CursorPath ToCursorPath(Document *doc, size_t offset);
void SetPageInfo(PageInfo *page);
void ViewInitialize(Display *aDisp, Window aWin, const char *aText, PageInfo *page);
bool ViewForwardCursor();
bool ViewBackwardCursor();
bool ViewUpwardCursor();
bool ViewDownwardCursor();

#endif
