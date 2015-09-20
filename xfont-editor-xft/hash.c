#include <stdio.h>
#include <sys/types.h>
#include <gc.h>
#include <string.h>
#include "hash.h"

// djb2
static unsigned long hash(const unsigned char *str, size_t bytes)
{
    unsigned long hash = 5381;
    int c;

    while (bytes--) {
	c = *str++;
	hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

Hash *HashCreateN(size_t n)
{
    Hash *h = GC_MALLOC(sizeof(Hash));
    h->bins = GC_MALLOC(sizeof(List *) * n);
    h->num_bins = n;
    return h;
}

void *HashGet(Hash *h, String key)
{
    size_t index = hash((const unsigned char*) key.buf, key.bytes) % h->num_bins;

    List *bin = h->bins[index];

    while (bin) {
	if (key.bytes == bin->key.bytes &&
	    strncmp(key.buf, bin->key.buf, key.bytes) == 0) {
	    return bin->data;
	}
	bin = bin->next;
    }
    return NULL;
}

void HashSet(Hash *h, String key, void *value)
{
    size_t index = hash((const unsigned char*) key.buf, key.bytes) % h->num_bins;

    List *bin = h->bins[index];

    if (bin == NULL) {
    Prepend:
	h->bins[index] = GC_MALLOC(sizeof(List));
	h->bins[index]->key = key;
	h->bins[index]->data = value;
	h->bins[index]->key.buf = GC_STRNDUP(key.buf, key.bytes);
	h->bins[index]->next = bin;
    } else {
	while (bin) {
	    if (key.bytes == bin->key.bytes &&
		memcmp(key.buf, bin->key.buf, key.bytes) == 0) {
		bin->data = value;
		return;
	    }
	    bin = bin->next;
	}
	bin = h->bins[index];
	goto Prepend;
    }
}

#if 0
int main()
{
    Hash *h = HashCreateN(1024);

    HashSet(h, (String) { "abc", 3 }, "def");
    printf("%s\n", (char*) HashGet(h, (String) { GC_STRDUP("abc"), 3 }));
}
#endif
