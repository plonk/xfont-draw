#ifndef VIEW_H
#define VIEW_H

#include <stdbool.h>
#include "document.h"

void ViewInitialize(Display *aDisp, Window aWin, const char *aText, PageInfo *page);
void ViewRedraw(void);
void ViewSetOption(const char *name, const char *value);
void ViewSetPageInfo(PageInfo *page);
bool ViewBackwardCursor(void);
bool ViewDownwardCursor(void);
bool ViewForwardCursor(void);
bool ViewUpwardCursor(void);

#endif
