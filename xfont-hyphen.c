/**
 * Helvetica で英文を表示するプログラム。両端揃え。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "util.h"
// time関数の為に time.h をインクルードする。
#include <time.h>
#include <ctype.h>
#include <assert.h>
// for execlp
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FONT_HELVETICA_14 "-adobe-helvetica-medium-r-*-*-14-*-*-*-*-*-iso8859-1"
// #define FONT_HELVETICA_14 "-adobe-times-medium-r-*-*-14-*-*-*-*-*-iso8859-1"
#define CHAR_HYPHEN '-'
#define PROGRAM_NAME "xfont-hyphen"

typedef struct {
    int width;
    size_t start;
    size_t length;
    int can_hyphenate;
} Token;

static Display *disp;
static Window win;
static GC gc;
static XFontStruct *font;

int IsSpace(const char *buff, Token token);

void UpdateTokenWidth(const char *buff, Token *tok, int line_final)
{
    int i;
    int width = 0;

    for (i = 0; i < tok->length; i++) {
	XCharStruct *info = GetCharInfo(font, buff[tok->start + i]);

	width += info->width;
    }
    if (line_final && tok->can_hyphenate)
    	width += GetCharInfo(font, CHAR_HYPHEN)->width;
    
    tok->width = width;
}

void DrawToken(const char *buff, Token token, int *x, int *y, int line_final)
{
    if (IsSpace(buff, token)) {
#if 0
	XFillRectangle(disp, win, gc, *x, *y - font->ascent,
		       token.width, font->ascent + font->descent);
#endif
	*x += token.width;
    } else {
	char word[1024];

	strncpy(word, buff + token.start, token.length);
	word[token.length] = '\0';
	int len = token.length;

	if (line_final && token.can_hyphenate) {
	    strcat(word, "-");
	    len ++ ;
	}

	XDrawString(disp, win, gc,
		    *x,	// X座標
		    *y,	// Y座標。ベースライン
		    word,
		    len);
	*x += token.width;
    }
}

int IsSpace(const char *buff, Token token)
{
    return isspace(buff[token.start]);
}

void DrawLine(int x, int y, const char *buff, Token tokens[], int ntokens) {
    // 行の描画
    puts("DrawLine");
    int i;

    for (i = 0; i < ntokens; i++) {
	int line_final = (i == ntokens - 1);
	DrawToken(buff, tokens[i], &x, &y, line_final);
    }
}

int CountSpaceTokens(const char *buff, Token tokens[], int ntok)
{
    int nspaces = 0;

    int i;
    for (i = 0; i < ntok; i++) {
	if (isspace(buff[tokens[i].start]))
	    nspaces++;
    }
    return nspaces;
}

int Hyphenate(const char *word, int length, int points[], int size)
{
    static int initialized = 0;
    static FILE *out, *in;
    static int outgoing[2];
    static int incoming[2];

    if (!initialized) {
	pipe(outgoing);
	pipe(incoming);
	pid_t res = fork();
	if (res == -1) {
	    abort();
	} else if (res == 0) {
	    close(outgoing[1]);
	    close(incoming[0]);

	    close(0);
	    close(1);

	    dup(outgoing[0]);
	    dup(incoming[1]);

	    execlp("ruby", "ruby", "hyph.rb", NULL);
	} else {
	    close(outgoing[0]);
	    close(incoming[1]);
	    out = fdopen(outgoing[1], "w");
	    in = fdopen(incoming[0], "r");
	    initialized = 1;
	}
    }

    char line[1024];

    fwrite(word, 1, length, out);
    fwrite("\n", 1, 1, out);
    fflush(out);

    fgets(line, 1024, in);

    char *ptr = line;

    int i = 0;
    while (*ptr != '\n') {
	long n = strtol(ptr, &ptr, 10);

	points[i] = (int) n;
	i++;
	if (i == size) {
	    break;
	}
    }
    return i;
}

#define TOKEN_STACK_SIZE 1000

int GetNextToken(const char *str, size_t start, size_t *end, int *can_hyphenate)
{
    static Token stack[TOKEN_STACK_SIZE];
    static int ntokens = 0;

    if (ntokens > 0) {
	assert( start == stack[ntokens - 1].start );
	*end = stack[ntokens - 1].start + stack[ntokens - 1].length;
	*can_hyphenate = stack[ntokens - 1].can_hyphenate;
	ntokens--;
	return 1;
    }

    size_t word_end;
    if (NextToken(str, start, &word_end) == 0)
	return 0;
    else if (isspace(str[start])) {
	goto returnWholeWord;
    } else {
	int points[20];
	int n;
	int i;
	int morpheme_end = word_end;

	n = Hyphenate(str + start, word_end - start, points, 20);
	printf("%d syllables\n", n+1);
	if (n > 0) {
	    // 逆順にスタックに詰む…
	    for (i = n - 1; i >= 0; morpheme_end = start + points[i], i--) {
		assert( ntokens < TOKEN_STACK_SIZE );
		stack[ntokens].start = start + points[i];
		stack[ntokens].length = morpheme_end - stack[ntokens].start;
		stack[ntokens].can_hyphenate = (i != n - 1); /* 考えなおす必要がある */
		stack[ntokens].width = -1;

		ntokens++;
	    }
	    // 最初の音節を返す。
	    *end = start + points[0];
	    *can_hyphenate = 1;
	    return 1;
	} else {
	returnWholeWord:
	    *end = word_end;
	    *can_hyphenate = 0;
	    return 1;
	}
    }
}

void InspectToken(const char *buff, Token tok)
{
    printf("#<Token '%.*s' "
	   "width=%d start=%zu length=%zu can_hyphenate=%d>\n",
	   (int) tok.length, &buff[tok.start],
	   tok.width, tok.start, tok.length, tok.can_hyphenate);
}

void DumpTokens(const char *buffer, Token tokens[], int ntok)
{
    int i;

    for (i = 0; i < ntok; i++) {
	InspectToken(buffer, tokens[i]);
    }
}


void Redraw(const char *buff)
{
    XWindowAttributes attrs;

    XGetWindowAttributes(disp, win, &attrs);

    const int LEFT_MARGIN = 50;
    const int LINE_HEIGHT = 20;
    const int RIGHT_MARGIN = attrs.width - LEFT_MARGIN;
    const int TOP_MARGIN = 50;

    if (RIGHT_MARGIN < 10) {
	fprintf(stderr, "Viewport size too small.\n");
	return;
    }

    XClearWindow(disp, win);
    size_t start = 0, next;
    int x = LEFT_MARGIN; // left margin
    int y = TOP_MARGIN + font->ascent;

#define MAX_TOKENS_PER_LINE 1024
    Token tokens[MAX_TOKENS_PER_LINE];
    int ntok = 0;
    int can_hyphenate;
    /**
     * トークンを切り出して、LEFT_MARGIN が埋まる直前まで tokens にトークンの情報を入れる。
     * 次に空白を表わすトークンが不足分(LEFT_MARGIN - x)を補うように引き伸ばす。
     * 行を表示して y を増やす。tokens をクリアする。
     * 入らなかったトークンが空白だった場合は省略、それ以外の場合は tokens[0] とする。
     */
    while (GetNextToken(buff, start, &next, &can_hyphenate)) {
	size_t len = next - start;
	Token tok;

	tok.start = start;
	tok.length = len;
	tok.can_hyphenate = can_hyphenate;
	UpdateTokenWidth(buff, &tok, 0);

    redoToken:
	if (isspace(buff[start])) { // 空白トークン
	    if (x == LEFT_MARGIN) // 行頭
		goto nextIter; // 無視する
	    else {
		goto tryAddToken;
	    }
	}
    tryAddToken:
	printf("tryAddToken: %d\n", (int) start);
	printf("tryAddToken: x = %d\n", x);
	if (x + tok.width > RIGHT_MARGIN && // 入らない
	    x != LEFT_MARGIN) { // 行の最初のトークンの場合は見切れてもよい
	    // 行の完成

	    // 1つ以上のトークンがあるので、tokens[ntok-1] へのアクセスは安全である。
	    assert(ntok > 0);

	    // 行末の空白を削除する
	    if (IsSpace(buff, tokens[ntok - 1])) {
		x -= tokens[ntok - 1].width;
		ntok--;
	    }

	    if (tokens[ntok - 1].can_hyphenate) {
		x -= tokens[ntok - 1].width;
		UpdateTokenWidth(buff, &tokens[ntok - 1], 1);
		x += tokens[ntok - 1].width;
	    }

	    DumpTokens(buff, tokens, ntok);

	    // 行の空白の数を数える。
	    int nspaces;
	    nspaces = CountSpaceTokens(buff, tokens, ntok);

	    // 空白の調整
	    if (nspaces > 0) {
		printf("%d spaces\n", nspaces);
		const int shortage = RIGHT_MARGIN - x;
		int alphas[MAX_TOKENS_PER_LINE];
		int j;
		int i_alpha;

		Distribute(shortage, nspaces, alphas);

		// 空白の引き伸ばしに上限を設ける。
		const int MAX_ALPHA = 6;
		
		i_alpha = 0;
		for (j = 0; j < ntok; j++) {
		    if (isspace(buff[tokens[j].start])) {
			int a = int_min(alphas[i_alpha], MAX_ALPHA);

			printf("adding %d to tokens[%d].width\n", a, j);
			tokens[j].width += a;
			i_alpha++;
		    }
		}
		assert(i_alpha == nspaces);
	    }

	    DrawLine(LEFT_MARGIN, y, buff, tokens, ntok);

	    y += LINE_HEIGHT;
	    x = LEFT_MARGIN;
	    ntok = 0; // トークン配列のクリア
	    goto redoToken;
	} else {	
	    printf("AddToken '%.*s...'\n", (int) tok.length, &buff[start]);
	    // トークンを追加する。
	    tokens[ntok] = tok;
	    x += tok.width;
	    ntok++;
	}
    nextIter:
	start = next;
    }
    DrawLine(LEFT_MARGIN, y, buff, tokens, ntok);
}

void Initialize()
{

    disp = XOpenDisplay(NULL); // open $DISPLAY
    win = XCreateSimpleWindow(disp,						// ディスプレイ
				DefaultRootWindow(disp),			// 親ウィンドウ
				0, 0,						// (x, y)
				640, 480,					// 幅・高さ
				0,						// border width
				0,						// border color
				WhitePixel(disp, DefaultScreen(disp)));	// background color
    XMapWindow(disp, win);

    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, gc,
		   BlackPixel(disp, DefaultScreen(disp)));

    // 暴露イベントを受け取る。
    XSelectInput(disp, win, ExposureMask);

    font = XLoadQueryFont(disp, FONT_HELVETICA_14);
    XSetFont(disp, gc, (font)->fid);
}

void CleanUp()
{
    XUnloadFont(disp, font->fid);
    XFreeGC(disp, gc);
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
}

void UsageExit()
{
    fprintf(stderr, "Usage: " PROGRAM_NAME " FILENAME\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
	UsageExit();

    const char *filepath = argv[1];

    FILE *fp = fopen(filepath, "r");
    if ( fp == NULL ) {
	perror(filepath);
	exit(1);
    }
    struct stat st;
    if ( stat(filepath, &st) == -1 ) {
	perror(filepath);
	exit(1);
    }
    char *text = alloca(st.st_size + 1);
    if ( fread(text, 1, st.st_size, fp) != st.st_size) {
	fprintf(stderr, "warning: size mismatch\n");
    }
    fclose(fp);

    Initialize();

    XEvent ev;

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	if (ev.type != Expose)
	    continue;

	Redraw(text);
    }

    CleanUp();
}
