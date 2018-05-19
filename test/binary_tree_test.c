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

const char *words[] =
  {"hi", "bye", "word", "test", "sequence", "license", "bsd", "3-clause", "best", "verify", "error", "word", "numeric", "alpha", "beta", "twice", "again", "more", "zoo", "the", "quick", "sly", "fox", "jumped", "over", "the", "two", "lazy", "dogs" };

int main(void) {
  struct binary_tree_t *bt;
  struct binary_tree_iterator_t *bti;
  char buf[256];
  const char *prev, *cur;
  int count, num;

  if ((bt = NewBinaryTree(NULL, NULL)) == NULL)
    exit(1);

  for (count = 0; count < sizeof(words) / sizeof(char *); count++) {
    if (BinaryTreeInsert(bt, words[count], NULL) < 0) {
      fprintf(stderr, "Cannot insert %s\n", words[count]);
      exit(1);
    }
    if (!BinaryTreeVerify(bt)) {
      fprintf(stderr, "Tree verification failed after inserting: %s\n", words[count]);
    }
  }
  
  if ((bti = NewBinaryTreeIterator(bt)) == NULL)
    exit(1);
  
  while (BinaryTreeIteratorNext(bti)) {
    puts(BinaryTreeIteratorKey(bti));
  }

  srand(3092);
  for (count = 0; count < 10000; count++) {
    snprintf(buf, sizeof(buf), "%08X", rand());
    
    if (BinaryTreeInsert(bt, buf, NULL) < 0) {
      fprintf(stderr, "Cannot insert %s\n", buf);
      exit(1);
    }
  }
  if (!BinaryTreeVerify(bt))
    fprintf(stderr, "Tree verification failed after numbers\n");

  num = BinaryTreeCount(bt);
  count = 0;
  prev = NULL;
  BinaryTreeIteratorReset(bti);
  while (BinaryTreeIteratorNext(bti)) {
    count++;
    cur = BinaryTreeIteratorKey(bti);
    //puts(cur);
    if (prev && strcmp(cur, prev) <= 0) {
      fprintf(stderr, "Out of order\n");
    }
    prev = cur;
  }

  if (num != count) {
    fprintf(stderr, "Incorrect count\n");
  }
  
  return 0;
}
