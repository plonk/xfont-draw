#ifndef HASH_H
#define HASH

typedef struct _String {
    const char *buf;
    size_t bytes;
} String;

typedef struct _List {
    String key;
    void *data;
    struct _List *next;
} List;

typedef struct _Hash {
    List **bins;
    size_t num_bins;
} Hash;

Hash *HashCreateN(size_t n);
void *HashGet(Hash *h, String key);
Hash *HashCreateN(size_t n);
void HashSet(Hash *h, String key, void *value);

#endif
