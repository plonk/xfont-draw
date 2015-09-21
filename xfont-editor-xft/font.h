#ifndef FONT_H
#define FONT_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "hash.h"

typedef struct
{
    Display *disp;
    Hash *glyph_widths;
    XftFont *xft_font;
} YFont;

// Fontconfig のフォント指定文字列から Font を作る。
YFont *YFontCreate(Display *disp, const char *font_description);
int YFontTextWidth(YFont *, const char *str, int bytes);
void YFontDestroy(YFont *);
double YFontEm(YFont *font);
void YFontTextExtents(YFont *font, const char *str, int bytes, XGlyphInfo *extents_return);

#endif
