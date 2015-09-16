#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include <memory>
#include <string>
#include <vector>

using namespace std;

class Element {
protected:
    XPoint origin;
public:
    virtual string str() = 0;
    virtual void draw(XftDraw *draw, XftColor *color, XftFont *font) { }
};

class Character : public Element {
    string m_str;

public:
    Character(const char* str) :
	m_str(str) { }

    virtual string str() {
	return m_str;
    }
    virtual void draw(XftDraw *draw, XftColor *color, XftFont *font) {
	XftDrawStringUtf8(draw, color, font, 50, 450, (unsigned char *) m_str.c_str(), 3);
    }
};

class Unbreakable : public Element {
    shared_ptr<Character> m_component;

public:
    Unbreakable(shared_ptr<Character> ch) {
	m_component = ch;
    }

    virtual string str() {
	return m_component->str();
    }
      // virtual XElementInfo extents() {
    // 	XftTextExtentsUtf8 (Display    *dpy,
    // 			    XftFont    *font,
    // 			    m_str.str(),
    // 			    int        len,
    // 			    XElementInfo *extents);
    // }
};

class Row : public Element {
    vector<shared_ptr<Unbreakable> > m_components;

    pair<shared_ptr<Row>, vector<shared_ptr<Unbreakable> > >
    fill(vector<shared_ptr<Unbreakable> > &words) {
    }
};

class Lexer {
    string m_text;

    static bool is_latin(string &ch) {
	char b = ch[0];
	return 0x21 <= b && b <= 0x7e;
    }

    static int byte_length(const char *utf8) {
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

    static vector<string> each_char(const char *utf8) {
	const char *p = utf8;
	vector<string> res;

	while (*p != '\0') {
	    int len = byte_length(p);
	    res.push_back(string(p, p + len));
	    p += len;
	}
	return res;
    }

public:
    Lexer(const char *utf8) :
	m_text(utf8) {
    }

    vector<shared_ptr<Unbreakable> > product() {
	if (m_text.empty()) return vector<shared_ptr<Unbreakable> >();

	auto v = each_char(m_text.c_str());
	string latin_buf = "";
	vector<shared_ptr<Unbreakable> > res;

	for (auto it = v.begin(); it != v.end(); it++) {
	    if (latin_buf == "") {
		latin_buf = *it;
	    } else { 
		if (is_latin(*it)) {
		    latin_buf += *it;
		} else {
		    res.push_back((shared_ptr<Unbreakable>) new Unbreakable((shared_ptr<Character>) new Character(latin_buf.c_str())));
		    latin_buf = "";
		    res.push_back((shared_ptr<Unbreakable>) new Unbreakable((shared_ptr<Character>) new Character(it->c_str())));
		}
	    }
	}
	if (latin_buf != "") {
	    res.push_back((shared_ptr<Unbreakable>) new Unbreakable((shared_ptr<Character>) new Character(latin_buf.c_str())));
	}
	return res;
    }
};

class Program {
    unsigned long m_white, m_black;
    Display* m_disp;
    Window m_win;
    GC m_gc;

    XftColor m_xft_black;
    XftDraw *m_draw;
    XftFont *m_font;

    Window CreateMainWindow() {
	return XCreateSimpleWindow(m_disp,			// ディスプレイ
				   DefaultRootWindow(m_disp),	// 親ウィンドウ
				   0, 0,			// (x, y)
				   640, 480,			// 幅・高さ
				   0,				// border width
				   0,				// border color
				   m_white);			// background color
    }

    void ShowXftVersion() {
	fprintf(stderr,
		"Xft version: %d.%d.%d\n",
		XFT_MAJOR, XFT_MINOR, XFT_REVISION);
    }

    void init() {
	ShowXftVersion();

	m_disp = XOpenDisplay(NULL); // open $DISPLAY

	m_white = WhitePixel(m_disp, DefaultScreen(m_disp));
	m_black = BlackPixel(m_disp, DefaultScreen(m_disp));

	m_win = CreateMainWindow();

	Atom WM_DELETE_WINDOW = XInternAtom(m_disp, "WM_DELETE_WINDOW", False); 
	XSetWMProtocols(m_disp, m_win, &WM_DELETE_WINDOW, 1);
	XMapWindow(m_disp, m_win);

	m_gc = XCreateGC(m_disp, m_win, 0, NULL);
	XSetForeground(m_disp, m_gc, m_black);
	// 暴露イベントを要求する。
	XSelectInput(m_disp, m_win, ExposureMask);

	m_font = XftFontOpenName(m_disp, DefaultScreen(m_disp),
				 "Source Han Sans JP-200:matrix=2 0 0 1");
	m_draw = XftDrawCreate(m_disp, m_win,
			       DefaultVisual(m_disp, DefaultScreen(m_disp)),
			       DefaultColormap(m_disp, DefaultScreen(m_disp)));

	XftColorAllocName(m_disp, DefaultVisual(m_disp, DefaultScreen(m_disp)),
			  DefaultColormap(m_disp, DefaultScreen(m_disp)),
			  "skyblue4",
			  &m_xft_black);
    }

    void draw() {
	auto doc = (shared_ptr<Element>) new Character("鱗");

	XClearWindow(m_disp, m_win);
	doc->draw(m_draw, &m_xft_black, m_font);
    }

public:
    int main(int argc, char *argv[]) {
	init();
	draw();

	XEvent ev;
	while (1) { // イベントループ
	    XNextEvent(m_disp, &ev);

	    fprintf(stderr, "event type = %d\n", ev.type);
	    switch (ev.type) {
	    case Expose:
		draw();
		break;

	    case ClientMessage:
		XDestroyWindow(m_disp, m_win);
		XCloseDisplay(m_disp);
		return 0;

	    default:
		;
	    }

	}
    }
};

int main(int argc, char *argv[]) {
    auto prog = (shared_ptr<Program>) new Program();

    return prog->main(argc, argv);
}
