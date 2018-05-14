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

#ifndef BINARY_TREE_H
#define BINARY_TREE_H

struct binary_tree_t;

struct binary_tree_t *NewBinaryTree(void *(*copy_func)(const void *), void (*free_func)(void *));
void FreeBinaryTree(struct binary_tree_t *bt);
struct binary_tree_t *CopyBinaryTree(const struct binary_tree_t *bt);

size_t BinaryTreeCount(const struct binary_tree_t *bt);
int BinaryTreeInsert(struct binary_tree_t *bt, const char *key, void *data);
const void *BinaryTreeLookup(const struct binary_tree_t *bt, const char *key, int *is_present);
int BinaryTreeRemove(struct binary_tree_t *bt, const char *key);
int BinaryTreeVerify(struct binary_tree_t *bt); /* For test */
void BinaryTreeForeach(struct binary_tree_t *bt, void (*func)(const char *, void **, void *), void *ref_data);

struct binary_tree_iterator_t;
struct binary_tree_iterator_t *NewBinaryTreeIterator(const struct binary_tree_t *bt);
void FreeBinaryTreeIterator(struct binary_tree_iterator_t *bti);
void BinaryTreeIteratorReset(struct binary_tree_iterator_t *bti);
int BinaryTreeIteratorNext(struct binary_tree_iterator_t *bti);
const char *BinaryTreeIteratorKey(struct binary_tree_iterator_t *bti);
const void *BinaryTreeIteratorData(struct binary_tree_iterator_t *bti);
void BinaryTreeIteratorSetData(struct binary_tree_iterator_t *bti, void *data);

#endif
