CC=gcc
CFLAGS=-g -Wall -std=c11 -I/usr/include/freetype2
VIEW_SRCS=color.c document.c hash.c util.c xfont-editor-xft-utf8.c xfont-editor-xft-view.c
VIEW_OBJS=$(VIEW_SRCS:.c=.o)
TARGETS=xfont-editor-xft draw
LIBS=-lXft -lX11 -lXext -lfontconfig -lgc
TOOLKIT_LIBS=-lXm -lXt

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(VIEW_OBJS) view.a

%.o: %.c
	$(CC) $(CFLAGS) -c $<

view.a: $(VIEW_OBJS)
	ar cr $@ $^

xfont-editor-xft: xfont-editor-xft.o view.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

draw: xfont-editor-xft.o view.a
	$(CC) $(LDFLAGS) -o $@ $^ $(TOOLKIT_LIBS) $(LIBS)
