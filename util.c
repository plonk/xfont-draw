#include "util.h"
#include <stdlib.h>
#include <stdio.h>

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
