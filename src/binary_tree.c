/*
  Copyright (C) 2018 Paul Maurer

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  
  1. Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
  
  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
  
  3. Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <string.h>

#include "binary_tree.h"

struct node_t {
  char *key;
  void *data;
  int height;
  struct node_t *left;
  struct node_t *right;
};

struct binary_tree_t {
  struct node_t *root;
  size_t count;
  void *(*copy_func)(const void *);
  void (*free_func)(void *);
};

struct binary_tree_t *NewBinaryTree(void *(*copy_func)(const void *), void (*free_func)(void *)) {
  struct binary_tree_t *bt;

  if ((bt = malloc(sizeof(*bt))) == NULL) {
    fprintf(stderr, "Cannot allocate memory for binary tree root\n");
    goto err;
  }
  memset(bt, 0, sizeof(*bt));
  bt->copy_func = copy_func;
  bt->free_func = free_func;
  
  return bt;
  
 err:
  return NULL;
}

static struct node_t *NewNode(const char *key, void *data) {
  struct node_t *n;

  if ((n = malloc(sizeof(*n))) == NULL) {
    fprintf(stderr, "Cannot allocate memory for binary tree node\n");
    goto err;
  }
  memset(n, 0, sizeof(*n));
  n->data = data;
  n->height = -1;
  
  if ((n->key = strdup(key)) == NULL) {
    fprintf(stderr, "Cannot allocate memory for binary tree node key\n");
    goto err2;
  }
  
  return n;
  
 err2:
  free(n);
 err:
  return NULL;
}

static void FreeNode(struct node_t *node, void (*free_func)(void *)) {
  if (node == NULL)
    return;
  
  free(node->key);
  if (free_func)
    free_func(node->data);
  FreeNode(node->left, free_func);
  FreeNode(node->right, free_func);
  free(node);
}

void FreeBinaryTree(struct binary_tree_t *bt) {
  if (bt == NULL)
    return;
  
  FreeNode(bt->root, bt->free_func);
  free(bt);
}

static struct node_t *CopyNode(struct node_t *src, const struct binary_tree_t *bt) {
  struct node_t *dest;
  
  if ((dest = NewNode(src->key, src->data)) == NULL)
    goto err;

  dest->height = src->height;
  if (bt->copy_func && (dest->data = bt->copy_func(src->data)) == NULL)
    goto err2;
  
  if (src->left && (dest->left = CopyNode(src->left, bt)) == NULL)
    goto err2;

  if (src->right && (dest->right = CopyNode(src->right, bt)) == NULL)
    goto err2;
  
  return dest;
  
 err2:
  FreeNode(dest, bt->free_func);
 err:
  return NULL;
}

struct binary_tree_t *CopyBinaryTree(const struct binary_tree_t *bt) {
  struct binary_tree_t *nbt;
  
  if ((nbt = NewBinaryTree(bt->copy_func, bt->free_func)) == NULL)
    goto err;
  nbt->count = bt->count;
  
  if (bt->root)
    if ((nbt->root = CopyNode(bt->root, bt)) == NULL)
      goto err2;

  return nbt;

 err2:
  FreeBinaryTree(nbt);
 err:
  return NULL;
}

size_t BinaryTreeCount(const struct binary_tree_t *bt) {
  return bt->count;
}

#define STACK_SZ (sizeof(size_t) * 8 + 3)

struct stack_t {
  struct node_t **stack[STACK_SZ];
  int depth;
};

#define STACK_CUR(st) ((st)->stack[(st)->depth-1])
#define STACK_PUSH(st, node) ((st)->stack[(st)->depth++] = node)
#define STACK_POP(st) ((st)->stack[--(st)->depth])
#define STACK_DECR(st) ((st)->depth--)

static int FindNode(struct stack_t *stack, const struct binary_tree_t *bt, const char *key) {
  struct node_t **cur = (struct node_t **) &bt->root;
  int ret;

  stack->depth = 0;
  STACK_PUSH(stack, cur);
  while (*cur != NULL) {
    if ((ret = strcmp(key, (*cur)->key)) == 0)
      return 1;
    else if (ret < 0)
      cur = &(*cur)->left;
    else
      cur = &(*cur)->right;
    STACK_PUSH(stack, cur);
  }
  
  return 0;
}

#define HEIGHT(n) ((n) ? ((n)->height) : -1)

static void FixHeight(struct node_t *n) {
  int lh, rh;

  if (n == NULL)
    return;
  
  lh = HEIGHT(n->left);
  rh = HEIGHT(n->right);
  n->height = (lh > rh ? lh : rh) + 1;
}

/*    x            z            y
 *   / \          / \         /   \
 *      z   to   x      or   x     z
 *     / \      / \         / \   / \
 *    y            y          yl yr
 */
static void RotateLeft(struct node_t **n) {
  struct node_t *x, *y, *z;
  
  x = *n;
  z = x->right;
  y = z->left;
  
  if (HEIGHT(y) > HEIGHT(z->right)) {
    x->right = y->left;
    z->left  = y->right;
    y->left  = x;
    y->right = z;
    *n = y;
    FixHeight(x);
    FixHeight(z);
    FixHeight(y);
  } else {
    x->right = y;
    z->left  = x;
    *n = z;
    FixHeight(x);
    FixHeight(z);
  }
}

static void RotateRight(struct node_t **n) {
  struct node_t *x, *y, *z;
  
  x = *n;
  z = x->left;
  y = z->right;
  
  if (HEIGHT(y) > HEIGHT(z->left)) {
    x->left  = y->right;
    z->right = y->left;
    y->right = x;
    y->left  = z;
    *n = y;
    FixHeight(x);
    FixHeight(z);
    FixHeight(y);
  } else {
    x->left  = y;
    z->right = x;
    *n = z;
    FixHeight(x);
    FixHeight(z);
  }
}

static void Rebalance(struct stack_t *stack) {
  struct node_t **n;
  int lh, rh, oh;

  while (stack->depth > 0) {
    n = STACK_POP(stack);
    oh = (*n)->height;
    lh = HEIGHT((*n)->left);
    rh = HEIGHT((*n)->right);
    if (lh > rh + 1) {
      RotateRight(n);
    } else if (rh > lh + 1) {
      RotateLeft(n);
    } else {
      (*n)->height = (lh > rh ? lh : rh) + 1;
    }
    if ((*n)->height == oh)
      return;
  }
}

const void *BinaryTreeLookup(const struct binary_tree_t *bt, const char *key, int *is_present) {
  struct stack_t st;
  
  if (FindNode(&st, bt, key)) {
    if (is_present)
      *is_present = 1;
    return (*STACK_CUR(&st))->data;
  }

  if (is_present)
    *is_present = 0;
  return NULL;
}

int BinaryTreeInsert(struct binary_tree_t *bt, const char *key, void *data) {
  struct stack_t st;
  struct node_t **n;
  int found;
  
  found = FindNode(&st, bt, key);
  n = STACK_CUR(&st);

  if (found) {
    if (bt->free_func)
      bt->free_func((*n)->data);
    (*n)->data = data;
    
    return 0;
  }

  if ((*n = NewNode(key, data)) == NULL)
    return -1;
  bt->count++;
  
  Rebalance(&st);
  return 1;
}

int BinaryTreeRemove(struct binary_tree_t *bt, const char *key) {
  struct stack_t st;
  struct node_t **n, *is, *cur;
  
  if (!FindNode(&st, bt, key))
    return 0;

  bt->count--;
  n = STACK_CUR(&st);
  cur = *n;
  if (cur->right == NULL) {
    st.depth--;
    *n = cur->left;
    free(cur->key);
    if (bt->free_func)
      bt->free_func(cur->data);
    free(cur);
    STACK_DECR(&st);
    Rebalance(&st);
    return 1;
  }

  n = &cur->right;
  STACK_PUSH(&st, n);
  while ((*n)->left) {
    n = &(*n)->left;
    STACK_PUSH(&st, n);
  }
  
  is = *n;
  free(cur->key);
  if (bt->free_func)
    bt->free_func(cur->data);
  cur->key  = is->key;
  cur->data = is->data;
  *n = is->right;
  free(is);
  
  STACK_DECR(&st);
  Rebalance(&st);
  return 1;
}

static int VerifyNode(struct node_t *n) {
  int lh, rh, xh, ret = 1;

  if (n == NULL)
    return ret;
  
  lh = HEIGHT(n->left);
  rh = HEIGHT(n->right);
  xh = (lh > rh ? lh : rh) + 1;
  if (n->height != xh) {
    ret = 0;
    fprintf(stderr, "Error, node has incorrect height: expected %d, found %d\n",
	    xh, n->height);
  }

  if (abs(lh - rh) >= 2) {
    ret = 0;
    fprintf(stderr, "Error, tree is not balanced: left = %d, right = %d\n",
	    lh, rh);    
  }
  
  ret = VerifyNode(n->left) && ret;
  ret = VerifyNode(n->right) && ret;
  
  return ret;
}

int BinaryTreeVerify(struct binary_tree_t *bt) {
  return VerifyNode(bt->root);
}

static void NodeForeach(struct node_t *n, void (*func)(const char *, void **, void *), void *ref_data) {
  if (n == NULL)
    return;

  NodeForeach(n->left, func, ref_data);
  func(n->key, &n->data, ref_data);
  NodeForeach(n->right, func, ref_data);
}

void BinaryTreeForeach(struct binary_tree_t *bt, void (*func)(const char *, void **, void *), void *ref_data) {
  NodeForeach(bt->root, func, ref_data);
}

struct binary_tree_iterator_t {
  struct binary_tree_t *bt;
  int init;
  int state[STACK_SZ];
  struct stack_t stack;
};

#define BTI_STATE(bti) ((bti)->state[(bti)->stack.depth - 1])
#define BTI_PUSH(bti, node)			\
  do {						\
    if (*(node)) {				\
      (bti)->state[(bti)->stack.depth] = 0;	\
      STACK_PUSH(&(bti)->stack, node);		\
    }						\
  } while (0)
#define BTI_CUR(bti) STACK_CUR(&(bti)->stack)
#define BTI_POP(bti) STACK_POP(&(bti)->stack)

struct binary_tree_iterator_t *NewBinaryTreeIterator(const struct binary_tree_t *bt) {
  struct binary_tree_iterator_t *bti;

  if ((bti = malloc(sizeof(*bti))) == NULL) {
    fprintf(stderr, "Could not allocate memory for binary tree iterator\n");
    goto err;
  }
  memset(bti, 0, sizeof(*bti));
  bti->bt = (struct binary_tree_t *) bt;

  return bti;
  
 err:
  return NULL;
}

void FreeBinaryTreeIterator(struct binary_tree_iterator_t *bti) {
  free(bti);
}

void BinaryTreeIteratorReset(struct binary_tree_iterator_t *bti) {
  bti->init = 0;
  bti->stack.depth = 0;
}

int BinaryTreeIteratorNext(struct binary_tree_iterator_t *bti) {
  struct node_t *cur;
  
  while (1) {
    if (bti->stack.depth <= 0) {
      if (bti->init)
	return 0;
      bti->init = 1;
      BTI_PUSH(bti, &bti->bt->root);
      continue;
    }

    cur = *BTI_CUR(bti);
    switch (BTI_STATE(bti)++) {
    case 0:
      BTI_PUSH(bti, &cur->left);
      continue;

    case 1:
      return 1;

    case 2:
      BTI_PUSH(bti, &cur->right);
      continue;

    case 3:
      bti->stack.depth--;
      continue;
    }
  }
}

const char *BinaryTreeIteratorKey(struct binary_tree_iterator_t *bti) {
  if (bti->stack.depth <= 0)
    return NULL;
  
  return (*STACK_CUR(&bti->stack))->key;
}

const void *BinaryTreeIteratorData(struct binary_tree_iterator_t *bti) {
  if (bti->stack.depth <= 0)
    return NULL;
  
  return (*STACK_CUR(&bti->stack))->data;
}

void BinaryTreeIteratorSetData(struct binary_tree_iterator_t *bti, void *data) {
  struct node_t *n;
  
  if (bti->stack.depth <= 0)
    return;
  
  n = *STACK_CUR(&bti->stack);
  if (bti->bt->free_func)
    bti->bt->free_func(n->data);
  n->data = data;
}
