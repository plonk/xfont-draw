CC=gcc
CFLAGS=-g -Wall -std=c11 -I/usr/include/freetype2
VIEW_SRCS=color.c document.c hash.c util.c utf8-string.c view.c
VIEW_OBJS=$(VIEW_SRCS:.c=.o)
TARGETS=editor draw
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

editor: editor.o view.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

draw: draw.o view.a
	$(CC) $(LDFLAGS) -o $@ $^ $(TOOLKIT_LIBS) $(LIBS)
