// カラーモジュール
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <gc.h>

typedef struct {
    char *name;
    XColor color;
} XColorEntry;

typedef struct {
    char *name;
    XftColor color;
} XftColorEntry;

#define MAX_COLORS 1024

static XColorEntry x_color_table[1024];
static size_t num_x_colors;

static XftColorEntry xft_color_table[1024];
static size_t num_xft_colors;

static Display *disp;
static Colormap colormap;
static Visual *visual;

void ColorInitialize(Display *aDisp)
{
    disp = aDisp;
    colormap = DefaultColormap(disp, DefaultScreen(disp));
    visual = DefaultVisual(disp, DefaultScreen(disp));
}

bool ColorIsInitialized()
{
    return disp != NULL;
}

unsigned long ColorGetPixel(const char *name)
{
    assert(ColorIsInitialized());

    XColor color;
    XParseColor(disp, colormap, name, &color);
    XAllocColor(disp, colormap, &color);

    return color.pixel;
}

XftColor *ColorGetXftColor(const char *name)
{
    for (int i = 0; i < num_xft_colors; i++) {
	if (strcmp(xft_color_table[i].name, name) == 0) {
	    return &xft_color_table[i].color;
	}
    }

    if (num_xft_colors == MAX_COLORS) {
	fprintf(stderr, "color table full");
	exit(1);
    }
    xft_color_table[num_xft_colors].name = GC_STRDUP(name);
    XftColorAllocName(disp, visual, colormap, name, &xft_color_table[num_xft_colors].color);
    return &xft_color_table[num_xft_colors++].color;
}
