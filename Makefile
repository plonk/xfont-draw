CFLAGS=-g -Wall -std=c99 -D_POSIX_SOURCE
CXXFLAGS=-std=c++11 -g -Wall

COMMANDS=xfont-draw xfont-info xfont-eng xfont-justify xfont-hyphen xfont-unicode-cpp xfont-pagination xfont-font-combining xfont-double-buffering xfont-input xfont-im xfont-xft

all: $(COMMANDS)

clean:
	rm -f $(COMMANDS)

font.o: font.c
	gcc $(CFLAGS) -c $<

jisx0208.o: jisx0208.c
	gcc $(CFLAGS) -c $<

xfont-im: xfont-im.c util.c jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lXext

xfont-input: xfont-input.c util.c jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lXext

xfont-double-buffering: xfont-double-buffering.c util.c jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lXext

xfont-font-combining: xfont-font-combining.c util.c jisx0208.o font.o
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-pagination: xfont-pagination.c util.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-unicode-cpp: xfont-unicode-cpp.cpp util.c
	g++ $(CXXFLAGS) -o $@ $^ -lX11

xfont-hyphen: xfont-hyphen.c util.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-justify: xfont-justify.c util.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-eng: xfont-eng.c util.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-draw: xfont-draw.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-xft: xfont-xft.cc
	g++ $(CXXFLAGS) -I/usr/include/freetype2 -o $@ $^ -lXft -lX11

xfont-info: xfont-info.c
	gcc $(CFLAGS) -o $@ $^ -lX11
