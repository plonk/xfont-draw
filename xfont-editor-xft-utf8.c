#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gc.h>

#include "xfont-editor-xft-utf8.h"

size_t Utf8CharBytes(const char *utf8) {
    unsigned char b = *utf8;

    if ((b & 0xc0) == 0x00 ||
	(b & 0xc0) == 0x40) {
	return 1;
    } else if ((b & 0xc0) == 0xc0) {
	if (b >> 1 == 126) {
	    return 6;
	} else if (b >> 2 == 62) {
	    return 5;
	} else if (b >> 3 == 30) {
	    return 4;
	} else if (b >> 4 == 14) {
	    return 3;
	} else if (b >> 5 == 6) {
	    return 2;
	}
    }
    abort();
}

// NUL文字でも停止しない。
const char *Utf8AdvanceChar(const char *utf8)
{
    return utf8 + Utf8CharBytes(utf8);
}

size_t Utf8CountChars(const char *utf8)
{
    size_t count;

    count = 0;
    while (*utf8 != '\0') {
	utf8 = Utf8AdvanceChar(utf8);
	count++;
    }

    return count;
}

size_t Utf8CountCharsBuffer(const char *utf8, size_t length)
{
    const char *p = utf8;
    size_t count = 0;

    while (p < utf8 + length) {
	p = Utf8AdvanceChar(p);
	count++;
    }

    return count;
}

char *ReadFile(const char *filepath)
{
    FILE *file;

    file = fopen(filepath, "r");
    if (file == NULL) {
	perror("fopen");
	exit(1);
    }

    fseek(file, 0, SEEK_END);

    long length;
    length = ftell(file);

    rewind(file);

    char *buf = GC_MALLOC(length);

    size_t bytes_read;
    bytes_read = fread(buf, 1, length, file);

    if (bytes_read != length) {
	fprintf(stderr, "file size mismatch");
	exit(1);
    }

    fclose(file);

    return buf;
}

static int IsAsciiPrintable(const char *p)
{
    return Utf8CharBytes(p) == 1 && *p >= 0x21 && *p <= 0x7e;
}

static int ctype(const char *utf8, int (*ctype_func)(int))
{
    return Utf8CharBytes(utf8) == 1 && ctype_func(utf8[0]);
}

static int IsForbiddenAtStart(const char *utf8)
{
    static const char chars[] = ",)]｝、〕〉》」』】〙〗〟’”｠»"
	"ゝゞーァィゥェォッャュョヮヵヶぁぃぅぇぉっゃゅょゎゕゖ"
	"ㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇷ゚ㇺㇻㇼㇽㇾㇿ々〻"
	"‐゠–〜～"
	"?!‼⁇⁈⁉"
	"・:;/"
	"。.";

    for (const char *p = chars; *p != '\0'; p = Utf8AdvanceChar(p)) {
	if (strncmp(utf8, p, Utf8CharBytes(p)) == 0)
	    return 1;
    }
    return 0;
}

// str の  start 位置からトークン(単語あるいは空白)を切り出す。
// トークンを構成する最後の文字の次の位置が *end に設定される。
// トークンが読み出せた場合は 1、さもなくば 0 を返す。
int NextTokenBilingual(const char *utf8, size_t start, size_t *end)
{
    const char *p = utf8 + start;

    /* トークナイズするものがない */
    if (*p == '\0')
	return 0;

    if (*p == ' ') {
	do {
	    p = Utf8AdvanceChar(p);
	} while (*p && *p == ' ');
    } else if (ctype(p, isalnum) || *p == '\'') {
	do  {
	    p = Utf8AdvanceChar(p);
	} while (*p && (ctype(p, isalnum) || *p == '\''));
    } else {
	p = Utf8AdvanceChar(p);
    }

    while (*p && IsForbiddenAtStart(p))
	p = Utf8AdvanceChar(p);

    *end = p - utf8;
    return 1;
}
