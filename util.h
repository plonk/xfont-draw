#ifndef UTIL_H
#define UTIL_H

#include <X11/Xlib.h>

XCharStruct *GetCharInfo(XFontStruct *font, unsigned char);
XCharStruct *GetCharInfo16(XFontStruct *font, unsigned char byte1, unsigned char byte2);
void InspectCharStruct(XCharStruct character);
int NextToken(const char *str, size_t start, size_t *end);
void Distribute(int m, size_t n, int a[]);

int int_max(int a, int b);
int int_min(int a, int b);

#endif
