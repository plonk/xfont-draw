#include "font.h"
#include <gc.h>

YFont *YFontCreate(Display *disp, const char *font_desc)
{
    YFont *font;

    font = GC_MALLOC(sizeof(YFont));
    font->disp = disp;
    font->glyph_widths = HashCreateN(4096);
    font->xft_font =
	XftFontOpenName(disp, DefaultScreen(disp), font_desc);

    if (font->xft_font == NULL) {
	font = NULL;
    }
    return font;
}

void YFontTextExtents(YFont *font, const char *str, int bytes, XGlyphInfo *extents_return)
{
    XftTextExtentsUtf8 (font->disp, font->xft_font, (FcChar8 *) str, bytes, extents_return);
}

static int TextWidthUncached(YFont *font, const char *str, int bytes)
{
    XGlyphInfo extents;

    XftTextExtentsUtf8
	(font->disp, font->xft_font, (FcChar8 *) str, bytes, &extents);
    return extents.xOff;
}

int YFontTextWidth(YFont *font, const char *str, int bytes)
{
    String key = { str, bytes };
    int *pWidth = HashGet(font->glyph_widths, key);

    if (!pWidth) {
	pWidth = GC_MALLOC(sizeof(int));
	*pWidth = TextWidthUncached(font, str, bytes);
	HashSet(font->glyph_widths, key, pWidth);
    }
    return *pWidth;
}

void YFontDestroy(YFont *font)
{
    XftFontClose(font->disp, font->xft_font);
}

double YFontEm(YFont *font)
{
    double em;

    FcPatternGetDouble(font->xft_font->pattern, FC_PIXEL_SIZE, 0, &em);
    return em;
}
