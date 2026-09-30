#ifndef CAVER_STD_VECTOR_H
#define CAVER_STD_VECTOR_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct std_vector {
	char *begin;
	char *end;
	char *cap;
} std_vector;

static inline void std_vector_push_back(std_vector *v, const void *elem, size_t esz) {
	if (v->end == v->cap) {
		size_t n = (v->end - v->begin) / esz;
		size_t nc = n ? n * 2 : 4;
		char *nb = (char *)malloc(nc * esz);
		if (n) {
			memcpy(nb, v->begin, n * esz);
		}
		free(v->begin);
		v->begin = nb;
		v->end = nb + n * esz;
		v->cap = nb + nc * esz;
	}
	memcpy(v->end, elem, esz);
	v->end += esz;
}

static inline size_t std_vector_size(const std_vector *v, size_t esz) {
	return (v && v->begin && v->end) ? (size_t)(v->end - v->begin) / esz : 0;
}

static inline void std_vector_clear(std_vector *v) {
	if (v) {
		v->end = v->begin;
	}
}

#endif // CAVER_STD_VECTOR_H
