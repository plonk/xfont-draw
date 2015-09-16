#ifndef COLOR_H
#define COLOR_H

void ColorInitialize(Display *aDisp);
bool ColorIsInitialized();
unsigned long ColorGetPixel(const char *name);
XftColor *ColorGetXftColor(const char *name);

#endif
