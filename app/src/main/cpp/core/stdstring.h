#ifndef LAWNCHER_STDSTRING_H
#define LAWNCHER_STDSTRING_H

#include <stddef.h>

/* I'm not *too* sure but the size should be 12b on 32 and 24b on 64.. */
#if defined(__arm__)
	#define STRING_SHORT_CAP 11
#else
	#define STRING_SHORT_CAP 23
#endif

typedef struct {
	size_t cap;
	size_t size;
	char *data;
} String_Long;

typedef struct {
	unsigned char size;
	char data[STRING_SHORT_CAP];
} String_Short;

typedef struct String {
	union {
		String_Long l;
		String_Short s;
	}; // sonion
} String;

static inline int String_isLong(const String *s) {
	return s->s.size & 1u;
}

static inline size_t String_size(const String *s) {
	return String_isLong(s) ? s->l.size : (s->s.size >> 1);
}

static inline const char *String_get(const String *s) { // basically s->cstr
	return String_isLong(s) ? s->l.data : s->s.data;
}

void String_create(String *out, const char *s);
void String_destroy(String *s);

#endif //LAWNCHER_STDSTRING_H
