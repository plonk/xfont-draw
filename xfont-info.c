#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>

Display *disp;

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

// 文字ごとのメトリクス情報を表示する
void InspectPerChar(XCharStruct per_char[],
		    unsigned min_char_or_byte2,
		    unsigned max_char_or_byte2,
		    unsigned min_byte1,
		    unsigned max_byte1)
{
    if (per_char == NULL)
	abort();

    if (min_byte1 == 0 && max_byte1 == 0) {
	// 1 バイトエンコーディング
	const int min_char = min_char_or_byte2;
	const int max_char = max_char_or_byte2;
	const int D = max_char - min_char + 1;
	int i;

	for (i = 0; i < D; i++) {
	    printf("char %d\n", i + min_char);
	    InspectCharStruct( per_char[i] );
	}
    } else {
	// 2 バイトエンコーディング
	const int min_byte2 = min_char_or_byte2;
	const int max_byte2 = max_char_or_byte2;
	const int D = max_byte2 - min_byte2 + 1;
	int i;

	for (i = 0; ; i++) {
	    unsigned char byte1 = i / D + min_byte1;
	    unsigned char byte2 = i % D + min_byte2;

	    printf("char %d/%d\n", byte1, byte2);
	    InspectCharStruct( per_char[i] );

	    if (byte1 == max_byte1 && byte2 == max_byte2)
		break;
	}
    }
}

char *string_properties[] = {
    "FONTNAME_REGISTRY",
    "FOUNDRY",
    "FAMILY_NAME",
    "WEIGHT_NAME",
    "SLANT",
    "SPACING",
    "CHARSET_REGISTRY",
    "CHARSET_ENCODING",
    "COPYRIGHT",
    "SETWIDTH_NAME",
    "ADD_STYLE_NAME",
    "FONT",
    "RESOLUTION",
    NULL
};

#include <stdbool.h>
#include <string.h>

bool IsStringProperty(const char *name)
{
    char **pp = string_properties;

    while (*pp != NULL) {
	if (strcmp(name, *pp) == 0) 
	    return true;
	pp++;
    } 
    return false;
}

void InspectProperties(XFontStruct *font,
		       XFontProp props[],
		       int n_properties)
{
    int i;

    for (i = 0; i < n_properties; i++) {
	const char *atom_name;

	atom_name = XGetAtomName(disp, props[i].name);
	printf("properties[%d]: %s\n", i, atom_name);
	if (IsStringProperty(atom_name)) {
	    printf("\t = \"%s\"\n", XGetAtomName(disp, props[i].card32));
	} else {
	    printf("\t = %lu\n", props[i].card32);
	}
    }
}


void InspectFontStruct(XFontStruct *font)
{
#define SHOW(fmt, member) printf(#member " = " fmt "\n", font->member)
    SHOW("%p", ext_data);
    SHOW("%lu", fid);
    SHOW("%u", direction);
    SHOW("%u", min_char_or_byte2);
    SHOW("%u", max_char_or_byte2);
    SHOW("%u", min_byte1);
    SHOW("%u", max_byte1);

    SHOW("%d", all_chars_exist);
    SHOW("%u", default_char);

    // フォントプロパティ
    {
	SHOW("%d", n_properties);
	InspectProperties(font,
			  font->properties,
			  font->n_properties);
    }
    printf("min_bounds:\n");
    InspectCharStruct(font->min_bounds);
    printf("max_bounds:\n");
    InspectCharStruct(font->max_bounds);

    if (font->per_char) {
	printf("font->per_char: \n");

	InspectPerChar(font->per_char,
		       font->min_char_or_byte2,
		       font->max_char_or_byte2,
		       font->min_byte1,
		       font->max_byte1);
    } else {
	printf("font->per_char = (nil)\n");
    }

    SHOW("%d", ascent);
    SHOW("%d", descent);
#undef SHOW
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
	fprintf(stderr, "Usage: x-font-info FONT_NAME\n");
	exit(1);
    }
    
    const char *fontName = argv[1];

    disp = XOpenDisplay(NULL); // open $DISPLAY

    XFontStruct *font = XLoadQueryFont(disp, fontName);

    if (font == NULL) {
	fprintf(stderr, "No font named “%s”\n", fontName);
	exit(1);
    }

    printf("Font: %s\n", fontName);
    InspectFontStruct(font);

    XCloseDisplay(disp);

    return 0;
}
