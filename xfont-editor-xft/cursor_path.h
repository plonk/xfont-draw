#ifndef CURSOR_PATH_H
#define CURSOR_PATH_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t line;
    size_t token;
    size_t character;
} CursorPath;

#include "document.h"

#endif
