/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * rbtree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/error.h"
#include "rbtree.h"

#include <stdlib.h>
#include <string.h>

#define IS_RED(n) ((n) && (n)->is_red)

#ifdef NO_CUSTOM_ALLOC
static void destroy_nodes_dfs(rbtree_node_t *n)
{
	rbtree_node_t *l, *r;

	if (n != NULL) {
		l = n->left;
		r = n->right;
		free(n);
		destroy_nodes_dfs(l);
		destroy_nodes_dfs(r);
	}
}
#else
static void destroy_nodes_dfs(rbtree_node_t *n)
{
	(void)n;
}
#endif

static void flip_colors(rbtree_node_t *n)
{
	n->is_red = !n->is_red;
	n->left->is_red = !n->left->is_red;
	n->right->is_red = !n->right->is_red;
}

static rbtree_node_t *rotate_right(rbtree_node_t *n)
{
	rbtree_node_t *x;

	x = n->left;
	n->left = x->right;
	x->right = n;

	x->is_red = x->right->is_red;
	x->right->is_red = 1;
	return x;
}

static rbtree_node_t *rotate_left(rbtree_node_t *n)
{
	rbtree_node_t *x;

	x = n->right;
	n->right = x->left;
	x->left = n;

	x->is_red = x->left->is_red;
	x->left->is_red = 1;
	return x;
}

static rbtree_node_t *subtree_balance(rbtree_node_t *n)
{
	if (IS_RED(n->right) && !IS_RED(n->left))
		n = rotate_left(n);

	if (IS_RED(n->left) && IS_RED(n->left->left))
		n = rotate_right(n);

	if (IS_RED(n->left) && IS_RED(n->right))
		flip_colors(n);

	return n;
}

static rbtree_node_t *subtree_insert(rbtree_t *tree, rbtree_node_t *root,
				     rbtree_node_t *new)
{
	if (root == NULL)
		return new;

	if (tree->key_compare(tree->key_context, new->data, root->data) < 0) {
		root->left = subtree_insert(tree, root->left, new);
	} else {
		root->right = subtree_insert(tree, root->right, new);
	}

	return subtree_balance(root);
}

static rbtree_node_t *mknode(rbtree_t *t, const void *key, const void *value)
{
	rbtree_node_t *node;

#ifdef NO_CUSTOM_ALLOC
	node = calloc(1, sizeof(*node) + t->key_size_padded + t->value_size);
#else
	node = mem_pool_allocate(t->pool);
#endif

	if (node == NULL)
		return NULL;

	node->value_offset = t->key_size_padded;
	node->is_red = 1;

	memcpy(node->data, key, t->key_size);
	memcpy(node->data + t->key_size_padded, value, t->value_size);
	return node;
}

static rbtree_node_t *copy_node(rbtree_t *nt, const rbtree_t *t,
				const rbtree_node_t *n)
{
	rbtree_node_t *out;

#ifdef NO_CUSTOM_ALLOC
	out = calloc(1, sizeof(*out) + t->key_size_padded + t->value_size);
#else
	out = mem_pool_allocate(nt->pool);
#endif

	if (out == NULL)
		return NULL;

	memcpy(out, n, sizeof(*n) + t->key_size_padded + t->value_size);
	out->left = NULL;
	out->right = NULL;

	if (n->left != NULL) {
		out->left = copy_node(nt, t, n->left);

		if (out->left == NULL) {
			destroy_nodes_dfs(out);
			return NULL;
		}
	}

	if (n->right != NULL) {
		out->right = copy_node(nt, t, n->right);

		if (out->right == NULL) {
			destroy_nodes_dfs(out);
			return NULL;
		}
	}

	return out;
}

int rbtree_init(rbtree_t *tree, size_t keysize, size_t valuesize,
		int(*key_compare)(const void *, const void *, const void *))
{
	size_t diff, size;

	memset(tree, 0, sizeof(*tree));
	tree->key_compare = key_compare;
	tree->key_size = keysize;
	tree->key_size_padded = keysize;
	tree->value_size = valuesize;

	/* make sure the value always has pointer alignment */
	diff = keysize % sizeof(void *);

	if (diff != 0) {
		diff = sizeof(void *) - diff;

		if (SZ_ADD_OV(tree->key_size_padded, diff,
			      &tree->key_size_padded)) {
			return SQFS_ERROR_OVERFLOW;
		}
	}

	/* make sure the node can store the offset */
	if (sizeof(size_t) > sizeof(sqfs_u32)) {
		if (tree->key_size_padded > 0x0FFFFFFFFUL)
			return SQFS_ERROR_OVERFLOW;
	}

	/* make sure the nodes fit in memory */
	size = sizeof(rbtree_node_t);

	if (SZ_ADD_OV(size, tree->key_size_padded, &size))
		return SQFS_ERROR_OVERFLOW;

	if (SZ_ADD_OV(size, tree->value_size, &size))
		return SQFS_ERROR_OVERFLOW;

#ifndef NO_CUSTOM_ALLOC
	/* initialize the underlying pool allocator */
	tree->pool = mem_pool_create(size);
	if (tree->pool == NULL)
		return SQFS_ERROR_ALLOC;
#endif
	return 0;
}

int rbtree_copy(const rbtree_t *tree, rbtree_t *out)
{
	memcpy(out, tree, sizeof(*out));
	out->root = NULL;

#ifndef NO_CUSTOM_ALLOC
	out->pool = mem_pool_create(sizeof(rbtree_node_t) +
				    tree->key_size_padded +
				    tree->value_size);
	if (out->pool == NULL)
		return SQFS_ERROR_ALLOC;
#endif

	if (tree->root != NULL) {
		out->root = copy_node(out, tree, tree->root);

		if (out->root == NULL) {
			memset(out, 0, sizeof(*out));
			return SQFS_ERROR_ALLOC;
		}
	}

	return 0;
}

void rbtree_cleanup(rbtree_t *tree)
{
#ifdef NO_CUSTOM_ALLOC
	destroy_nodes_dfs(tree->root);
#else
	mem_pool_destroy(tree->pool);
#endif
	memset(tree, 0, sizeof(*tree));
}

int rbtree_insert(rbtree_t *tree, const void *key, const void *value)
{
	rbtree_node_t *node = mknode(tree, key, value);

	if (node == NULL)
		return SQFS_ERROR_ALLOC;

	tree->root = subtree_insert(tree, tree->root, node);
	tree->root->is_red = 0;
	return 0;
}

rbtree_node_t *rbtree_lookup(const rbtree_t *tree, const void *key)
{
	rbtree_node_t *node = tree->root;
	int ret;

	while (node != NULL) {
		ret = tree->key_compare(tree->key_context, key, node->data);
		if (ret == 0)
			break;

		node = ret < 0 ? node->left : node->right;
	}

	return node;
}
