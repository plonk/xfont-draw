#ifndef COLOR_H
#define COLOR_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

void ColorInitialize(Display *aDisp);
bool ColorIsInitialized();
unsigned long ColorGetPixel(const char *name);
XftColor *ColorGetXftColor(const char *name);

#endif
