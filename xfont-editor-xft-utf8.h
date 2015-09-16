#ifndef XFONT_EDITOR_XFT_UTF8_H
#define XFONT_EDITOR_XFT_UTF8_H

#include <sys/types.h>

const char *Utf8AdvanceChar(const char *utf8);
size_t Utf8CharBytes(const char *utf8);
size_t Utf8CountCharsBuffer(const char *utf8, size_t length);
size_t Utf8CountChars(const char *utf8);
int NextTokenBilingual(const char *utf8, size_t start, size_t *end);
char *ReadFile(const char *filepath);

#endif
