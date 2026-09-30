#include <stdlib.h>
#include <string.h>
#include "AchievementsManager.h"
#include "stdstring.h"
#include "log.h"
#include "ml.h"

#define LOG_TAG "AchievementsStuff"

// (4, 8)
#define PS sizeof(void *)
#define SS (3 * PS)
#define KEY_OFF archSplit(0x10, 0x20)
#define NODE_SIZE (KEY_OFF + SS + 2 * PS)
#define CB_OWNERS PS
#define CB_WEAK (PS + 4)
#define CB_PTR (PS + 8)

// Achievement
#define A_ID 0
#define A_TITLE SS
#define A_DESC (2 * SS)
#define A_POINTS (3 * SS)
#define A_STAT archSplit(0x28, 0x50)
#define A_GOAL archSplit(0x34, 0x68)

#define M_LIST archSplit(0x00, 0x00)
#define M_BYID archSplit(0x0c, 0x18)
#define M_KEYS archSplit(0x18, 0x30)
#define M_BYKEY archSplit(0x24, 0x48)

/* These should be moved to core/... */

typedef struct Node {
	struct Node *left, *right, *parent;
	unsigned char black;
} Node;

typedef struct {
	Node *begin;
	Node *root;
	size_t size;
} Tree;

typedef struct {
	char *begin, *end, *cap;
} Vec;

static void noop(void) {}
static void *fakeVtbl[6] = {noop, noop, noop, noop, noop, noop};

#define nodeKey(n) ((String *)((char *)(n) + KEY_OFF))

static int strCmp(const String *a, const String *b) {
	size_t la = String_size(a), lb = String_size(b);
	int c = memcmp(String_get(a), String_get(b), la < lb ? la : lb);
	return c ? c : (la < lb ? -1 : la > lb);
}

static int isLeft(Node *x) { return x == x->parent->left; }

static void rotL(Node *x) {
	Node *y = x->right;
	x->right = y->left;
	if (x->right) x->right->parent = x;
	y->parent = x->parent;
	if (isLeft(x)) x->parent->left = y; else x->parent->right = y;
	y->left = x;
	x->parent = y;
}

static void rotR(Node *x) {
	Node *y = x->left;
	x->left = y->right;
	if (x->left) x->left->parent = x;
	y->parent = x->parent;
	if (isLeft(x)) x->parent->left = y; else x->parent->right = y;
	y->right = x;
	x->parent = y;
}

static void balance(Node *root, Node *x) {
	x->black = x == root;
	while (x != root && !x->parent->black) {
		int l = isLeft(x->parent);
		Node *y = l ? x->parent->parent->right : x->parent->parent->left;
		if (y && !y->black) {
			x = x->parent;
			x->black = 1;
			x = x->parent;
			x->black = x == root;
			y->black = 1;
		} else {
			if (l != 0 == !isLeft(x)) {
				x = x->parent;
				if (l) rotL(x); else rotR(x);
			}
			x = x->parent;
			x->black = 1;
			x = x->parent;
			x->black = 0;
			if (l) rotR(x); else rotL(x);
			break;
		}
	}
}

static void treeInsert(Tree *t, const char *key, void *ptr, void *ctrl, int multi) {
	String tmp;
	String_create(&tmp, key);
	Node *parent = (Node *)&t->root;
	Node **slot = &t->root;
	for (Node *n = t->root; n;) {
		int c = strCmp(&tmp, nodeKey(n));
		if (c == 0 && !multi) break;
		parent = n;
		slot = c < 0 ? &n->left : &n->right;
		n = *slot;
	}
	String_destroy(&tmp);
	if (*slot) return;
	Node *n = calloc(1, NODE_SIZE);
	String_create(nodeKey(n), key);
	*(void **)((char *)n + KEY_OFF + SS) = ptr;
	*(void **)((char *)n + KEY_OFF + SS + PS) = ctrl;
	n->parent = parent;
	*slot = n;
	if (t->begin->left) t->begin = t->begin->left;
	balance(t->root, n);
	t->size++;
}

static void vecPush(Vec *v, const void *elem, size_t esz) {
	if (v->end == v->cap) {
		size_t n = (v->end - v->begin) / esz, nc = n ? n * 2 : 4;
		char *nb = malloc(nc * esz);
		if (n) memcpy(nb, v->begin, n * esz);
		free(v->begin);
		v->begin = nb;
		v->end = nb + n * esz;
		v->cap = nb + nc * esz;
	}
	memcpy(v->end, elem, esz);
	v->end += esz;
}

void AM_AddAchievement(AchievementsManager *m, const char *id, const char *title, const char *description, int points, const char *counter_name, int counter_value) {
	if (!counter_name) counter_name = "";
	if (!counter_value) counter_value = 1;

	char *mb = (char *)m;
	Achievement *a = calloc(1, sizeof(Achievement));
	char *ab = (char *)a;

	String_create((String *)(ab + A_ID), id);
	String_create((String *)(ab + A_TITLE), title);
	String_create((String *)(ab + A_DESC), description);
	String_create((String *)(ab + A_STAT), counter_name);
	*(int *)(ab + A_POINTS) = points;
	*(int *)(ab + A_GOAL) = counter_value;

	char *cb = calloc(1, 4 * PS);
	*(void **)cb = fakeVtbl;
	*(int *)(cb + CB_OWNERS) = 0x40000000;
	*(int *)(cb + CB_WEAK) = 0x40000000;
	*(void **)(cb + CB_PTR) = a;

	void *sp[2] = {a, cb};
	vecPush((Vec *)(mb + M_LIST), sp, sizeof sp);
	treeInsert((Tree *)(mb + M_BYID), id, a, cb, 0);

	const char *key = *counter_name ? counter_name : id;
	Tree *byKey = (Tree *)(mb + M_BYKEY);
	int seen = 0;
	String k;
	String_create(&k, key);
	for (Node *n = byKey->root; n && !seen;) {
		int c = strCmp(&k, nodeKey(n));
		if (c == 0) seen = 1; else n = c < 0 ? n->left : n->right;
	}
	if (!seen) vecPush((Vec *)(mb + M_KEYS), &k, SS);
	else String_destroy(&k);
	treeInsert(byKey, key, a, cb, 1);
}

static void freeNodes(Node *n) {
	if (!n) return;
	freeNodes(n->left);
	freeNodes(n->right);
	String_destroy(nodeKey(n));
	free(n);
}

void AM_ClearAchievements(AchievementsManager *m) {
	char *mb = (char *)m;

	Vec *list = (Vec *)(mb + M_LIST);
	for (char *p = list->begin; p < list->end; p += 2 * PS) {
		Achievement *a = *(Achievement **)p;
		void *cb = *(void **)(p + PS);
		char *ab = (char *)a;
		String_destroy((String *)(ab + A_ID));
		String_destroy((String *)(ab + A_TITLE));
		String_destroy((String *)(ab + A_DESC));
		String_destroy((String *)(ab + A_STAT));
		free(a);
		free(cb);
	}
	list->end = list->begin;

	Vec *keys = (Vec *)(mb + M_KEYS);
	for (char *p = keys->begin; p < keys->end; p += SS) {
		String_destroy((String *)p);
	}
	keys->end = keys->begin;

	Tree *byId = (Tree *)(mb + M_BYID);
	freeNodes(byId->root);
	byId->root = NULL;
	byId->begin = (Node *)&byId->root;
	byId->size = 0;

	Tree *byKey = (Tree *)(mb + M_BYKEY);
	freeNodes(byKey->root);
	byKey->root = NULL;
	byKey->begin = (Node *)&byKey->root;
	byKey->size = 0;
}

HOOK_SYMBOL(
	AchievementsManager_Constructor,
	"_ZN5Caver19AchievementsManagerC2Ev",
	void, (AchievementsManager *this)
) {
	orig_AchievementsManager_Constructor(this);
	AM_ClearAchievements(this);
//	ML_LoadAchievements(this);
}
