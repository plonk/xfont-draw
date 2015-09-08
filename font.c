#include <iconv.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "jisx0208.h"

// #define LATIN1_FONT	"-yoteichi-kay-medium-r-normal--16-13-75-75-c-70-iso8859-1"
// #define LATIN1_FONT	"-adobe-times-medium-r-*-*-24-*-*-*-*-*-iso8859-1"
#define LATIN1_FONT	"-adobe-times-medium-r-*-*-14-*-*-*-*-*-iso8859-1"
#define JISX0208_FONT	"-misc-fixed-medium-r-normal--14-130-75-75-c-140-jisx0208.1983-0"
// #define JISX0208_FONT	"-jis-fixed-medium-r-normal-*-24-*-*-*-c-*-jisx0208.1983-0"
#define UNI_FONT	"-gnu-unifont-medium-r-normal-sans-16-160-75-75-c-80-iso10646-1"

static XFontStruct *Latin1Font;
static XFontStruct *Jisx0208Font;
static XFontStruct *UniFont;
static Display *associated_display;

static iconv_t cd_to_latin1;
static iconv_t cd_to_eucjp;

void InitializeFonts(Display *disp)
{
    associated_display = disp;

    Latin1Font = XLoadQueryFont(disp, LATIN1_FONT);
    if (!Latin1Font) {
	fprintf(stderr, LATIN1_FONT "\n");
	abort();
    }

    Jisx0208Font = XLoadQueryFont(disp, JISX0208_FONT);
    if (!Jisx0208Font) {
	fprintf(stderr, JISX0208_FONT "\n");
	abort();
    }

    UniFont = XLoadQueryFont(disp, UNI_FONT);
    if (!UniFont) {
	fprintf(stderr, UNI_FONT "\n");
	abort();
    }

    cd_to_latin1 = iconv_open("ISO-8859-1", "UCS-2BE");
    if (cd_to_latin1 == (iconv_t) -1) {
	perror("ISO-8859-1");
	abort();
    }

    cd_to_eucjp = iconv_open("EUC-JP", "UCS-2BE");
    if (cd_to_eucjp == (iconv_t) -1) {
	perror("EUC-JP");
	abort();
    }
}

// 名前がわるい
void ShutdownFonts()
{
    XFreeFont(associated_display, Latin1Font);
    XFreeFont(associated_display, Jisx0208Font);
    XFreeFont(associated_display, UniFont);
    Latin1Font = Jisx0208Font = UniFont = NULL;
    iconv_close(cd_to_latin1);
    iconv_close(cd_to_eucjp);
}

static bool IsLatin1(XChar2b ucs2)
{
    char outbuf[1];
    char *pout = outbuf;
    char *pin = (char*) &ucs2;
    size_t inbytesleft = 2;
    size_t outbytesleft = 1;

    if ( iconv(cd_to_latin1, &pin, &inbytesleft, &pout, &outbytesleft) == -1 )
	return false;
    return true;
}

static bool IsJisx0208(XChar2b ucs2)
{
    char outbuf[3];
    char *pout = outbuf;
    char *pin = (char*) &ucs2;
    size_t inbytesleft = 2;
    size_t outbytesleft = 3;

    // EUC-JP でエンコードできなければ false。
    if ( iconv(cd_to_eucjp, &pin, &inbytesleft, &pout, &outbytesleft) == -1 )
	return false;

    // 1 バイトあるいは 3 バイトならば JIS X 0208 ではない。
    if (outbytesleft == 2 || outbytesleft == 0)
	return false;

    assert(outbytesleft == 1);
    assert((outbuf[0] & 0x80) && (outbuf[1] & 0x80));
    // MSB を落として、JIS X 0208 の範囲に入っていれば true。

    unsigned char byte1 = outbuf[0] & (~0x80);
    unsigned char byte2 = outbuf[1] & (~0x80);

    if (!(0x21 <= byte1 && byte1 <= 0x7e))
	return false;
    if (!(0x21 <= byte2 && byte2 <= 0x7e))
	return false;

    int ku = byte1 - 0x20;
    if (9 <= ku && ku <= 15)
	return false;
    if (ku > 84)
	return false;

    unsigned int cp = (byte1 << 8) | byte2;

    if ((0x222f <= cp && cp <= 0x2239) ||
	(0x2242 <= cp && cp <= 0x2249) ||
	(0x2251 <= cp && cp <= 0x225b) ||
	(0x226b <= cp && cp <= 0x2271) ||
	(0x227a <= cp && cp <= 0x227d) ||
	(0x2321 <= cp && cp <= 0x232f) ||
	(0x233a <= cp && cp <= 0x2340) ||
	(0x235b <= cp && cp <= 0x2360) ||
	(0x237b <= cp && cp <= 0x237e) ||
	(0x2474 <= cp && cp <= 0x247e) ||
	(0x2577 <= cp && cp <= 0x257e) ||
	(0x2639 <= cp && cp <= 0x2640) ||
	(0x2659 <= cp && cp <= 0x267e) ||
	(0x2742 <= cp && cp <= 0x2750) ||
	(0x2772 <= cp && cp <= 0x277e) ||
	(0x2841 <= cp && cp <= 0x287e) ||
	(0x4f54 <= cp && cp <= 0x4f7e) ||
	(0x7427 <= cp && cp <= 0x747e))
	return false;

    return true;
}

static XChar2b ToLatin1(XChar2b ucs2)
{
    char outbuf[1];
    char *pout = outbuf;
    char *pin = (char*) &ucs2;
    size_t inbytesleft = 2;
    size_t outbytesleft = 1;

    if ( iconv(cd_to_latin1, &pin, &inbytesleft, &pout, &outbytesleft) == -1 ) {
	perror("latin1");
	abort();
    }
    XChar2b res = { .byte1 = 0, .byte2 = outbuf[0] };
    return res;
}

static XChar2b ToJisx0208(XChar2b ucs2)
{
    size_t idx = ((size_t) ucs2.byte1 << 8) | ucs2.byte2;
    XChar2b jis;

    uint16_t jiscode = UnicodeToJisx0208[idx];
    jis.byte1 = jiscode >> 8;
    jis.byte2 = jiscode & 0xff;
    return jis;
}

XFontStruct *SelectFont(XChar2b ucs2, XChar2b *ch_return)
{
    if (ucs2.byte1 == 0 && iscntrl(ucs2.byte2)) {
	*ch_return = ucs2;
	return UniFont;
    } else if (IsLatin1(ucs2)) {
	*ch_return = ToLatin1(ucs2);
	return Latin1Font;
    } else if (IsJisx0208(ucs2)) {
	*ch_return = ToJisx0208(ucs2);
	return Jisx0208Font;
    } else {
	*ch_return = ucs2;
	return UniFont;
    }
}
