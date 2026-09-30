#ifndef CAVER_STD_TREE_H
#define CAVER_STD_TREE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "stdstring.h"

typedef struct std_tree_node {
	struct std_tree_node *left;
	struct std_tree_node *right;
	struct std_tree_node *parent;
	unsigned char black;
} std_tree_node;

typedef struct std_tree {
	std_tree_node *begin;
	std_tree_node *root;
	size_t size;
} std_tree;

static inline int std_tree_str_cmp(const String *a, const String *b) {
	size_t la = String_size(a), lb = String_size(b);
	int c = memcmp(String_get(a), String_get(b), la < lb ? la : lb);
	return c ? c : (la < lb ? -1 : la > lb);
}

static inline int std_tree_node_is_left(std_tree_node *x) {
	return x == x->parent->left;
}

static inline void std_tree_rotate_left(std_tree_node *x) {
	std_tree_node *y = x->right;
	x->right = y->left;
	if (x->right) x->right->parent = x;
	y->parent = x->parent;
	if (std_tree_node_is_left(x)) x->parent->left = y; else x->parent->right = y;
	y->left = x;
	x->parent = y;
}

static inline void std_tree_rotate_right(std_tree_node *x) {
	std_tree_node *y = x->left;
	x->left = y->right;
	if (x->left) x->left->parent = x;
	y->parent = x->parent;
	if (std_tree_node_is_left(x)) x->parent->left = y; else x->parent->right = y;
	y->right = x;
	x->parent = y;
}

static inline void std_tree_balance(std_tree_node *root, std_tree_node *x) {
	x->black = (x == root);
	while (x != root && !x->parent->black) {
		int l = std_tree_node_is_left(x->parent);
		std_tree_node *y = l ? x->parent->parent->right : x->parent->parent->left;
		if (y && !y->black) {
			x = x->parent;
			x->black = 1;
			x = x->parent;
			x->black = (x == root);
			y->black = 1;
		} else {
			if ((l != 0) == (!std_tree_node_is_left(x))) {
				x = x->parent;
				if (l) std_tree_rotate_left(x); else std_tree_rotate_right(x);
			}
			x = x->parent;
			x->black = 1;
			x = x->parent;
			x->black = 0;
			if (l) std_tree_rotate_right(x); else std_tree_rotate_left(x);
			break;
		}
	}
}

#endif // CAVER_STD_TREE_H
