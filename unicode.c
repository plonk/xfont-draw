static wchar_t to_codepoint(unsigned char *buf, size_t len)
{
    unsigned char *p = buf;
    wchar_t result = 0;

    if ((*p & 0xc0) == 0x00 ||
        (*p & 0xc0) == 0x40) {

        return (wchar_t) *p;
    } else if ((*p & 0xc0) == 0xc0) {
        size_t cnt = -1;

        if (*p >> 1 == 126) {
            result = *p & 1;
            cnt = 5;
        } else if (*p >> 2 == 62) {
            result = *p & 3;
            cnt = 4;
        } else if (*p >> 3 == 30) {
            result = *p & 7;
            cnt = 3;
        } else if (*p >> 4 == 14) {
            result = *p & 15;
            cnt = 2;
        } else if (*p >> 5 == 6) {
            result = *p & 31;
            cnt = 1;
        }

        if (cnt == -1 || cnt != len - 1) {
            return '?';
        }

        while (++p - buf <= cnt) {
            result <<= 6;
            result |= (*p & 63);
        }

        return result;
    } else {
        return (wchar_t) '?';
    }
}
