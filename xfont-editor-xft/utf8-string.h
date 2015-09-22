#ifndef UTF8_STRING_H
#define UTF8_STRING_H

#include <sys/types.h>

//// 文字クラス定義
// 始め括弧類
#define CC_OPEN_PAREN "「『（"
// 終わり括弧類
#define CC_CLOSE_PAREN "」』）"
// 読点類
#define CC_COMMA "、，"
// 句点類
#define CC_PERIOD "。．"
// 中点類
#define CC_MIDDLE_DOT "・：；"

//// プロトタイプ宣言
int CTypeOf(const char *utf8, int (*ctype_func)(int));
char *Format(const char *fmt, ...);
int IsForbiddenAtEnd(const char *utf8);
int IsForbiddenAtStart(const char *utf8);
int NextTokenBilingual(const char *utf8, size_t start, size_t *end);
char *ReadFile(const char *filepath);
char *StringConcat(const char *strings[]);
const char *Utf8AdvanceChar(const char *utf8);
size_t Utf8CharBytes(const char *utf8);
size_t Utf8CountChars(const char *utf8);
size_t Utf8CountCharsBuffer(const char *utf8, size_t length);
int Utf8IsAnyOf(const char *utf8, const char *klass);

#endif
