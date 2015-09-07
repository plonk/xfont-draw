CFLAGS=-g -Wall
CXXFLAGS=-g -fpermissive

COMMANDS=xfont-draw xfont-info xfont-eng xfont-justify xfont-hyphen xfont-unicode-cpp xfont-pagination xfont-font-combining xfont-double-buffering

all: $(COMMANDS)

clean:
	rm -f $(COMMANDS)

font.o: font.c
	gcc $(CFLAGS) -c $<

jisx0208.o: jisx0208.c
	gcc $(CFLAGS) -c $<

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

xfont-info: xfont-info.c
	gcc $(CFLAGS) -o $@ $^ -lX11
