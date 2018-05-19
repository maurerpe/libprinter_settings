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

#include "ps_stack.h"
#include "ps_ostream.h"

#ifdef DEBUG
#define PRINT_VALUE(msg, stack)				\
  do {							\
    struct ps_ostream_t *os;				\
							\
    if ((os = PS_NewStrOStream()) == NULL)		\
      break;						\
    							\
    if (PS_WriteValue(os, stack) < 0) {			\
      PS_FreeOStream(os);				\
      break;						\
    }							\
    							\
    printf(msg ": %s\n", PS_OStreamContents(os));	\
    							\
    PS_FreeOStream(os);					\
  } while (0);
#else
#define PRINT_VALUE(msg, stack)
#endif

struct ps_value_t *PS_NewStack(void) {
  struct ps_value_t *a, *b, *c;

  if ((a = PS_NewList()) == NULL)
    goto err;
  
  if ((b = PS_NewList()) == NULL)
    goto err2;

  if ((c = PS_NewList()) == NULL)
    goto err3;

  if (PS_AppendToList(b, c) < 0)
    goto err4;

  if (PS_AppendToList(a, b) < 0)
    goto err3;
  
  PRINT_VALUE("Alloc stack", a);
  return a;
  
 err4:
  PS_FreeValue(c);
 err3:
  PS_FreeValue(b);
 err2:
  PS_FreeValue(a);
 err:
  return NULL;
}

size_t PS_StackLength(struct ps_value_t *stack) {
  return PS_ItemCount(PS_GetItem(stack, -1)) - 1;
}

size_t PS_StackArgLength(struct ps_value_t *stack) {
  return PS_ItemCount(PS_GetItem(PS_GetItem(stack, -1), -1)) - 1;
}

int PS_ExpandStack(struct ps_value_t *stack, size_t new_level) {
  struct ps_value_t *ss, *fill;
  int ret;

  ss = PS_GetItem(stack, -1);
  if (PS_ItemCount(ss) - 1 > new_level)
    return -1;
  
  if ((fill = PS_NewFunction(NULL)) == NULL)
    return -1;
  
  ret = PS_ResizeList(ss, new_level + 1, fill);

  PS_FreeValue(fill);
  PRINT_VALUE("Expand stack", stack);
  return ret;
}

int PS_PushStack(struct ps_value_t *stack, struct ps_value_t *v) {
  int ret;
  ret = PS_AppendToList(PS_GetItem(PS_GetItem(stack, -1), -1), v);
  PRINT_VALUE("Push stack", stack);
  return ret;
}

struct ps_value_t *PS_CollapseStack(struct ps_value_t *stack, ssize_t new_level) {
  struct ps_value_t *expr, *top, *first;
  ssize_t count;

  expr = NULL;
  first = PS_GetItem(stack, -1);
  for (count = PS_StackLength(stack); count > new_level; count--) {
    if ((top = PS_PopFromList(first)) == NULL)
      goto err;

    if (PS_ItemCount(top) == 0) {
      PS_FreeValue(top);
      continue;
    }

    if (expr) {
      if (PS_AppendToList(top, expr) < 0)
	goto err2;
    }
    expr = top;
  }
  
  PRINT_VALUE("Collapse stack", stack);
  PRINT_VALUE("Collapse expr", expr);
  return expr;
  
 err2:
  PS_FreeValue(top);
 err:
  PS_FreeValue(expr);
  return NULL;
}

int PS_OpenGrouping(struct ps_value_t *stack, const char *func_name) {
  struct ps_value_t *a, *b;
  
  if ((a = PS_NewList()) == NULL)
    goto err;
  
  if (func_name)
    b = PS_NewFunction(func_name);
  else
    b = PS_NewList();
  if (b == NULL)
    goto err2;
  
  if (PS_AppendToList(a, b) < 0)
    goto err3;
  
  if (PS_AppendToList(stack, a) < 0)
    goto err2;
  
  PRINT_VALUE("Open grouping stack", stack);
  return 0;
  
 err3:
  PS_FreeValue(b);
 err2:
  PS_FreeValue(a);
 err:
  return -1;
}

struct ps_value_t *PS_CloseGrouping(struct ps_value_t *stack, int final, int *was_func) {
  struct ps_value_t *expr, *v;

  if (final) {
    if (PS_ItemCount(stack) != 1) {
      fprintf(stderr, "Not enough close parenthesis\n");
      goto err;
    }
  } else if (PS_ItemCount(stack) <= 1) {
    fprintf(stderr, "Too many close parenthesis\n");
    goto err;
  }
  
  if ((expr = PS_CollapseStack(stack, -1)) == NULL)
    goto err;
  
  PS_FreeValue(PS_PopFromList(stack));
  
  PRINT_VALUE("Close grouping stack", stack);
  if (PS_GetType(expr) == t_function) {
    if (was_func)
      *was_func = 1;
    return expr;
  }
  
  if (was_func)
    *was_func = 0;
  
  if (PS_ItemCount(expr) != 1) {
    if (final)
      fprintf(stderr, "Base expression must contain exactly one argument\n");
    else
      fprintf(stderr, "Parenthetical expressions must contain exactly one argument\n");
    goto err2;
  }

  if ((v = PS_AddRef(PS_GetItem(expr, 0))) == NULL)
    goto err2;
  
  PS_FreeValue(expr);
  return v;
  
 err2:
  PS_FreeValue(expr);
 err:
  return NULL;
}
