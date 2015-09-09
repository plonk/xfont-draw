#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// m を n 個の整数l a[0] ~ a[n-1] に分割する。
void Distribute(int m, size_t n, int a[])
{
    size_t i;

    if (m < 0) {
	// 負数は扱いたくない。
	for (i = 0; i < n; i++)
	    a[i] = 0;
	return;
    }

    int q = m / n;

    for (i = 0; i < n; i++)
	a[i] = q;

    int r = m % n;

    i = 0;
    while (r > 0) {
	a[i]++;
	r--;
	i = (i + 1) % n;
    }
}

// str の  start 位置からトークン(単語あるいは空白)を切り出す。
// トークンを構成する最後の文字の次の位置が *end に設定される。
// トークンが読み出せた場合は 1、さもなくば 0 を返す。
int NextToken(const char *str, size_t start, size_t *end)
{
    size_t index = start;

    /* トークナイズするものがない */
    if (str[index] == '\0')
	return 0;

    if (isspace(str[index])) {
	index++;
    } else {
	index++;
	while (str[index] && !isspace(str[index]))
	    index++;
    }
    *end = index;
    return 1;
}

XCharStruct *GetCharInfo(XFontStruct *font, unsigned char byte)
{
    return &font->per_char[byte + font->min_char_or_byte2];
}

XCharStruct *GetCharInfo16(XFontStruct *font, unsigned char byte1, unsigned char byte2)
{
    // N = (max_byte2 - min_byte2 + 1) * (byte1 + min_byte1) - (byte2 - min_byte2)
    const int min_byte1 = font->min_byte1;
    const int max_byte1 = font->max_byte1;
    const int min_byte2 = font->min_char_or_byte2;
    const int max_byte2 = font->max_char_or_byte2;

    // 範囲チェック
    if ( !(byte1 >= min_byte1 && byte1 <= max_byte1 &&
	   byte2 >= min_byte2 && byte2 <= max_byte2) ) {
	abort();
    }

    const int index_into_per_char = 
	(max_byte2 - min_byte2 + 1) * (byte1 - min_byte1) + (byte2 - min_byte2);
    return &font->per_char[index_into_per_char];
}

void InspectCharStruct(XCharStruct character)  /* struct copy */
{
#define SHOW(fmt, member) printf("\t" #member " = " fmt "\n", character.member)
    SHOW("%hd", lbearing);
    SHOW("%hd", rbearing);
    SHOW("%hd", width);
    SHOW("%hd", ascent);
    SHOW("%hd", descent);
    SHOW("%hu", attributes);
#undef SHOW
}

int int_max(int a, int b)
{
    return (a > b) ? a : b;
}

int int_min(int a, int b)
{
    return (a < b) ? a : b;
}
