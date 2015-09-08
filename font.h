#ifndef FONT_H
#define FONT_H

XFontStruct *SelectFont(XChar2b ucs2, XChar2b *ch_return);
void InitializeFonts(Display *disp);
void ShutdownFonts();

#endif

