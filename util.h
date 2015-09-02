#ifndef UTIL_H
#define UTIL_H

#include <X11/Xlib.h>

XCharStruct *GetCharInfo(XFontStruct *font, unsigned char);
XCharStruct *GetCharInfo16(XFontStruct *font, unsigned char byte1, unsigned char byte2);
void InspectCharStruct(XCharStruct character);

#endif
