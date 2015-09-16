CFLAGS=-g -Wall -std=c99 -D_POSIX_SOURCE
CXXFLAGS=-std=c++11 -g -Wall

COMMANDS=xfont-draw xfont-info xfont-eng xfont-justify xfont-hyphen \
	xfont-unicode-cpp xfont-pagination xfont-font-combining \
	xfont-double-buffering xfont-input xfont-im xfont-xft xfont-draw-xft \
	xfont-eng-xft xfont-justify-xft xfont-editor-xft

all: $(COMMANDS)

clean:
	rm -f $(COMMANDS)

color.o: color.c
	gcc $(CFLAGS) -I/usr/include/freetype2 -c $<

font.o: font.c
	gcc $(CFLAGS) -c $<

jisx0208.o: jisx0208.c
	gcc $(CFLAGS) -c $<

xfont-editor-xft-view.o: xfont-editor-xft-view.c
	gcc $(CFLAGS) -I/usr/include/freetype2 -c $<

xfont-editor-xft-utf8.o: xfont-editor-xft-utf8.c
	gcc $(CFLAGS) -c $<

xfont-im: xfont-im.c util.o jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lXext

xfont-input: xfont-input.c util.o jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lXext

xfont-double-buffering: xfont-double-buffering.c util.o jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lXext

xfont-font-combining: xfont-font-combining.c util.o jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-pagination: xfont-pagination.c util.o
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-unicode-cpp: xfont-unicode-cpp.cpp util.c
	g++ $(CXXFLAGS) -o $@ $^ -lX11

xfont-hyphen: xfont-hyphen.c util.o
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-justify: xfont-justify.c util.o
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-eng: xfont-eng.c util.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-draw: xfont-draw.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-xft: xfont-xft.cc
	g++ $(CXXFLAGS) -I/usr/include/freetype2 -o $@ $^ -lXft -lX11

xfont-draw-xft: xfont-draw-xft.c
	gcc $(CFLAGS) -I/usr/include/freetype2 -o $@ $^ -lXft -lX11

xfont-eng-xft: xfont-eng-xft.c util.c
	gcc $(CFLAGS) -I/usr/include/freetype2 -o $@ $^ -lXft -lX11

xfont-justify-xft: xfont-justify-xft.c util.o color.o
	gcc $(CFLAGS) -I/usr/include/freetype2 -o $@ $^ -lXft -lX11 -lXext -lgc

xfont-editor-xft: xfont-editor-xft.c util.o color.o xfont-editor-xft-utf8.o xfont-editor-xft-view.o
	gcc $(CFLAGS) -I/usr/include/freetype2 -o $@ $^ -lXft -lX11 -lXext -lgc

xfont-info: xfont-info.c
	gcc $(CFLAGS) -o $@ $^ -lX11

util.o: util.c
	gcc $(CFLAGS) -c $^
