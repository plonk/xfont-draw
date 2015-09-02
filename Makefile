CFLAGS=-Wall

all: xfont-draw xfont-info xfont-eng

xfont-eng: xfont-eng.c util.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-draw: xfont-draw.c
	gcc $(CFLAGS) -o $@ $^ -lX11

xfont-info: xfont-info.c
	gcc $(CFLAGS) -o $@ $^ -lX11
