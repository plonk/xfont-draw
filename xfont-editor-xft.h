#ifndef XFONT_EDITOR_XFT_H
#define XFONT_EDITOR_XFT_H

typedef struct {
    size_t line;
    size_t token;
    size_t character;
} CursorPath;

typedef struct {
    short x;
    short width;
    char *utf8;
    size_t length;
} Character;

typedef struct {
    short x;
    short width;
    Character *chars;
    size_t nchars;
} Token;

typedef struct  {
    Token *tokens;
    size_t ntokens;
} VisualLine;

typedef struct {
    short width, height;
    short margin_top, margin_right, margin_bottom, margin_left;
} PageInfo;

typedef struct {
    VisualLine *lines;
    size_t nlines;
    PageInfo *page;
    size_t max_offset;
} Document;

#endif
