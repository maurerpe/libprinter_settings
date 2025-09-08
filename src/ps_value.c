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
#include "ps_value.h"

struct list_head_t {
  struct ps_value_t **v;
  size_t num_alloc;
  size_t num_elem;
};

struct ps_value_t {
  enum ps_type_t type;
  size_t ref_count;
  
  union {
    int64_t v_integer;
    double v_float;
    char *v_string;
    struct list_head_t *v_list;
    struct binary_tree_t *v_object;
  } v;
};

static struct ps_value_t ps_const_null = {t_null, SIZE_MAX >> 1, {0}};
static struct ps_value_t ps_const_false = {t_boolean, SIZE_MAX >> 1, {0}};
static struct ps_value_t ps_const_true = {t_boolean, SIZE_MAX >> 1, {1}};

static struct ps_value_t *NewValue(enum ps_type_t type) {
  struct ps_value_t *ps;

  if ((ps = malloc(sizeof(*ps))) == NULL) {
    perror("Cannot allocate memory for printer settings value");
    goto err;
  }
  memset(ps, 0, sizeof(*ps));
  ps->type = type;

  return ps;
  
 err:
  return NULL;
}

struct ps_value_t *PS_NewNull(void) {
  return &ps_const_null;
}

struct ps_value_t *PS_NewBoolean(int v) {
  return (v ? &ps_const_true : &ps_const_false);
}

struct ps_value_t *PS_NewInteger(int64_t v) {
  struct ps_value_t *ps;
  
  if ((ps = NewValue(t_integer)) == NULL)
    return NULL;
  
  ps->v.v_integer = v;
  return ps;
}

struct ps_value_t *PS_NewFloat(double v) {
  struct ps_value_t *ps;
  
  if ((ps = NewValue(t_float)) == NULL)
    return NULL;
  
  ps->v.v_float = v;
  return ps;
}

struct ps_value_t *PS_NewString(const char *v) {
  if (v == NULL)
    return NULL;
  
  return PS_NewStringLen(v, strlen(v));
}

struct ps_value_t *PS_NewStringLen(const char *v, size_t len) {
  struct ps_value_t *ps;
  
  if (v == NULL)
    goto err;
  
  if ((ps = NewValue(t_string)) == NULL)
    goto err;
  
  if ((ps->v.v_string = malloc(len + 1)) == NULL) {
    perror("Could not allocate memory for printer settings string");
    goto err2;
  }
  memcpy(ps->v.v_string, v, len);
  ps->v.v_string[len] = '\0';
  
  return ps;
  
 err2:
  free(ps);
 err:
  return NULL;
}

struct ps_value_t *PS_NewVariable(const char *v) {
  struct ps_value_t *ps;

  if ((ps = PS_NewString(v)) == NULL)
    goto err;

  ps->type = t_variable;
  
  return ps;
  
 err:
  return NULL;
}

struct ps_value_t *PS_NewVariableLen(const char *v, size_t len) {
  struct ps_value_t *ps;

  if ((ps = PS_NewStringLen(v, len)) == NULL)
    goto err;

  ps->type = t_variable;
  
  return ps;
  
 err:
  return NULL;
}

struct ps_value_t *PS_NewBuiltinFunc(const char *v) {
  struct ps_value_t *ps;

  if ((ps = PS_NewString(v)) == NULL)
    goto err;

  ps->type = t_builtin_func;
  
  return ps;
  
 err:
  return NULL;
}

struct ps_value_t *PS_NewBuiltinFuncLen(const char *v, size_t len) {
  struct ps_value_t *ps;

  if ((ps = PS_NewStringLen(v, len)) == NULL)
    goto err;

  ps->type = t_builtin_func;
  
  return ps;
  
 err:
  return NULL;
}

#define INIT_LIST_SZ 16

struct ps_value_t *PS_NewList(void) {
  struct ps_value_t *ps;
  
  if ((ps = NewValue(t_list)) == NULL)
    goto err;
  
  if ((ps->v.v_list = malloc(sizeof(*ps->v.v_list))) == NULL) {
    perror("Could not allocate memory for printer settings list");
    goto err2;
  }
  memset(ps->v.v_list, 0, sizeof(*ps->v.v_list));
  
  if ((ps->v.v_list->v = calloc(INIT_LIST_SZ, sizeof(struct ps_value_t *))) == NULL) {
    perror("Could not allocate memory for printer settings list elements");
    goto err3;
  }
  ps->v.v_list->num_alloc = INIT_LIST_SZ;
  
  return ps;

 err3:
  free(ps->v.v_list);
 err2:
  free(ps);
 err:
  return NULL;
}

struct ps_value_t *PS_NewFunction(const struct ps_value_t *func) {
  struct ps_value_t *ps, *nn;

  if ((ps = PS_NewList()) == NULL)
    goto err;

  ps->type = t_function;
  if (func) {
    if ((nn = PS_CopyValue(func)) == NULL)
      goto err2;

    if (PS_AppendToList(ps, nn) < 0)
      goto err3;
  }
  
  return ps;

 err3:
  PS_FreeValue(nn);
 err2:
  PS_FreeValue(ps);
 err:
  return NULL;
}

static void *CopyVoid(const void *v) {
  return PS_CopyValue((const struct ps_value_t *) v);
}

static void FreeVoid(void *v) {
  PS_FreeValue((struct ps_value_t *) v);
}

struct ps_value_t *PS_NewObject(void) {
  struct ps_value_t *ps;

  if ((ps = NewValue(t_object)) == NULL)
    goto err;
  
  if ((ps->v.v_object = NewBinaryTree(CopyVoid, FreeVoid)) == NULL)
    goto err2;
  
  return ps;
  
 err2:
  free(ps);
 err:
  return NULL;
}

void PS_FreeValue(struct ps_value_t *v) {
  struct ps_value_t **cur, **end;
  
  if (v == NULL)
    return;
  
  if (v->type == t_null || v->type == t_boolean)
    return;
  
  if (v->ref_count-- > 0)
    return;
  
  switch (v->type) {
  case t_string:
  case t_variable:
  case t_builtin_func:
    free(v->v.v_string);
    break;
    
  case t_list:
  case t_function:
    cur = v->v.v_list->v;
    end = cur + v->v.v_list->num_elem;
    for (; cur < end; cur++)
      PS_FreeValue(*cur);
    free(v->v.v_list->v);
    free(v->v.v_list);
    break;
    
  case t_object:
    FreeBinaryTree(v->v.v_object);
    break;

  default:
    break;
  }
  
  free(v);
}

enum ps_type_t PS_GetType(const struct ps_value_t *v) {
  if (v == NULL)
    return t_null;
  
  return v->type;
}

int PS_IsNumeric(const struct ps_value_t *v) {
  if (v == NULL)
    return 0;

  return v->type == t_boolean || v->type == t_integer || v->type == t_float;
}

int PS_IsScalar(const struct ps_value_t *v) {
  if (v == NULL)
    return 1;
  
  return !(v->type == t_list || v->type == t_function || v->type == t_object);
}

int PS_AsBoolean(const struct ps_value_t *v) {
  return PS_AsInteger(v);
}

int64_t PS_AsInteger(const struct ps_value_t *v) {
  if (v == NULL)
    return 0;
  
  switch (v->type) {
  case t_null:
    return 0;
    
  case t_boolean:
  case t_integer:
    return v->v.v_integer;
    
  case t_float:
    return v->v.v_float;

  case t_string:
    return strtod(v->v.v_string, NULL);

  case t_variable:
  case t_builtin_func:
    return 0;

  default:
    return PS_ItemCount(v);
  }
}

double PS_AsFloat(const struct ps_value_t *v) {
  if (v == NULL)
    return 0.0;
  
  switch (v->type) {
  case t_float:
    return v->v.v_float;

  case t_string:
    return strtod(v->v.v_string, NULL);

  default:
    return PS_AsInteger(v);
  }
}

const char *PS_GetString(const struct ps_value_t *str_var) {
  if (str_var == NULL)
    return NULL;
  
  if (str_var->type != t_string &&
      str_var->type != t_variable &&
      str_var->type != t_builtin_func)
    return NULL;

  return str_var->v.v_string;
}

size_t PS_ItemCount(const struct ps_value_t *list_func_obj) {
  if (list_func_obj == NULL)
    return 0;
  
  if (list_func_obj->type == t_object)
    return BinaryTreeCount(list_func_obj->v.v_object);
  
  if (list_func_obj->type == t_list || list_func_obj->type == t_function) {
    return list_func_obj->v.v_list->num_elem;
  }
  
  if (list_func_obj->type == t_null)
    return 0;
  
  return 1;
}

struct ps_value_t *PS_GetItem(const struct ps_value_t *list, ssize_t pos) {
  if (list == NULL ||
      (list->type != t_list && list->type != t_function))
    return NULL;
  
  if (pos < 0)
    pos += list->v.v_list->num_elem;
  
  if (pos < 0 || pos >= list->v.v_list->num_elem)
    return NULL;

  return list->v.v_list->v[pos];
}

struct ps_value_t *PS_GetMember(const struct ps_value_t *obj, const char *name, int *is_present) {
  if (obj == NULL || name == NULL || obj->type != t_object)
    return NULL;
  
  return (struct ps_value_t *) BinaryTreeLookup(obj->v.v_object, name, is_present);
}

static struct ps_value_t *CopyList(const struct ps_value_t *v) {
  struct ps_value_t *ps, **cur, **end, *copy;
  
  if ((ps = PS_NewList()) == NULL)
    goto err;
  ps->type = v->type;
  
  cur = v->v.v_list->v;
  end = cur + v->v.v_list->num_elem;
  for (; cur < end; cur++) {
    if ((copy = PS_CopyValue(*cur)) == NULL)
      goto err2;
    
    if ((PS_AppendToList(ps, copy)) < 0)
      goto err3;
  }
  
  return ps;
  
 err3:
  PS_FreeValue(copy);
 err2:
  PS_FreeValue(ps);
 err:
  return NULL;
}

struct ps_value_t *PS_AddRef(const struct ps_value_t *v) {
  if (v == NULL)
    return NULL;
  
  if (v->type == t_null || v->type == t_boolean)
    return (struct ps_value_t *) v;
  
  if (v->ref_count == SIZE_MAX)
    return NULL;
  
  ((struct ps_value_t *) v)->ref_count++;
  
  return (struct ps_value_t *) v;
}

/* Deep copy mutable types, add refence to immutable types */
struct ps_value_t *PS_CopyValue(const struct ps_value_t *v) {
  struct ps_value_t *ps;

  if (v == NULL)
    return NULL;
  
  switch (v->type) {
  case t_string:
  case t_variable:
  case t_builtin_func:
    if ((ps = NewValue(v->type)) == NULL)
      return NULL;
    if ((ps->v.v_string = strdup(v->v.v_string)) == NULL) {
      free(ps);
      return NULL;
    }
    break;
    
  case t_list:
  case t_function:
    return CopyList(v);
    
  case t_object:
    if ((ps = NewValue(t_object)) == NULL)
      return NULL;
    if ((ps->v.v_object = CopyBinaryTree(v->v.v_object)) == NULL) {
      free(ps);
      return NULL;
    }
    break;
    
  default:
    if ((ps = PS_AddRef(v)) == NULL)
      return NULL;
    break;
  }
  
  return ps;
}

void PS_StringToVariable(struct ps_value_t *v) {
  if (v == NULL || v->type != t_string)
    return;

  v->type = t_variable;
}
void PS_VariableToString(struct ps_value_t *v) {
  if (v == NULL || v->type != t_variable)
    return;

  v->type = t_string;
}

int PS_AppendToString(struct ps_value_t *str, const char *append) {
  size_t len1, len2, tot;
  char *nstr;
  
  if (str == NULL || append == NULL)
    return -1;
  
  if (str->type != t_string &&
      str->type != t_variable &&
      str->type != t_builtin_func)
    return -1;

  len1 = strlen(str->v.v_string);
  len2 = strlen(append);
  tot  = len1 + len2;

  if ((nstr = realloc(str->v.v_string, tot + 1)) == NULL)
    return -1;
  
  memcpy(nstr + len1, append, len2);
  nstr[tot] = '\0';
  str->v.v_string = nstr;
  
  return 0;
}

static int GrowList(struct list_head_t *list) {
  struct ps_value_t **v;
  size_t new_alloc;

  new_alloc = list->num_alloc << 1;
  if (new_alloc < list->num_alloc)
    return -1;

  if ((v = calloc(new_alloc, sizeof(struct ps_value_t **))) == NULL)
    return -1;
  
  memcpy(v, list->v, list->num_elem * sizeof(struct ps_value_t **));
  free(list->v);
  list->v = v;
  
  return 0;
}

int PS_AppendToList(struct ps_value_t *list, struct ps_value_t *v) {
  struct list_head_t *head;
  
  if (list == NULL || v == NULL)
    return -1;
  
  if (list->type != t_list && list->type != t_function)
    return -1;
  
  head = list->v.v_list;
  if (head->num_elem >= head->num_alloc)
    if (GrowList(head) < 0)
      return -1;

  head->v[head->num_elem++] = v;
  
  return 0;
}

int PS_AppendCopyToList(struct ps_value_t *list, const struct ps_value_t *v) {
  struct ps_value_t *copy;
  
  if ((copy = PS_CopyValue(v)) == NULL)
    goto err;

  if (PS_AppendToList(list, copy) < 0)
    goto err2;

  return 0;
  
 err2:
  PS_FreeValue(copy);
 err:
  return -1;
}

struct ps_value_t *PS_PopFromList(struct ps_value_t *list) {
  if (list == NULL)
    return NULL;
  
  if (list->type != t_list && list->type != t_function)
    return NULL;
  
  if (list->v.v_list->num_elem == 0)
    return NULL;
  
  return list->v.v_list->v[--list->v.v_list->num_elem];
}

int PS_PrependToList(struct ps_value_t *list, struct ps_value_t *v) {
  struct list_head_t *head;
  
  if (list == NULL || v == NULL)
    return -1;
  
  if (list->type != t_list && list->type != t_function)
    return -1;
  
  head = list->v.v_list;
  if (head->num_elem >= head->num_alloc)
    if (GrowList(head) < 0)
      return -1;

  memmove(head->v + 1, head->v, head->num_elem * sizeof(struct ps_value_t *));
  head->v[0] = v;
  head->num_elem++;
  
  return 0;
}

int PS_SetItem(struct ps_value_t *list, size_t pos, struct ps_value_t *v) {
  struct list_head_t *head;
  
  if (list == NULL || v == NULL)
    return -1;
  
  if (list->type != t_list && list->type != t_function)
    return -1;
  
  if (pos >= list->v.v_list->num_elem)
    return -1;
  
  head = list->v.v_list;
  PS_FreeValue(head->v[pos]);
  head->v[pos] = v;
  
  return 0;
}

int PS_ResizeList(struct ps_value_t *list, size_t new_size, const struct ps_value_t *fill) {
  struct list_head_t *head;
  size_t count;
  
  if (list == NULL)
    goto err;
  
  if (list->type != t_list && list->type != t_function)
    goto err;
  
  if (fill == NULL && list->v.v_list->num_elem < new_size)
    goto err;
  
  head = list->v.v_list;
  
  while (head->num_alloc < new_size)
    if (GrowList(head) < 0)
      goto err;
  
  for (count = new_size; count < head->num_elem; count++)
    PS_FreeValue(head->v[count]);

  for (count = head->num_elem; count < new_size; count++)
    if ((head->v[count] = PS_CopyValue(fill)) == NULL)
      goto err2;
  
  head->num_elem = new_size;
  
  return 0;

 err2:
  while (--count >= head->num_elem)
    PS_FreeValue(head->v[count]);
 err:
  return -1;
}

int PS_AddMember(struct ps_value_t *obj, const char *name, struct ps_value_t *v) {
  if (obj == NULL || name == NULL || v == NULL)
    return -1;
  
  if (obj->type != t_object)
    return -1;
  
  return BinaryTreeInsert(obj->v.v_object, name, v);
}

int PS_RemoveMember(struct ps_value_t *obj, const char *name) {
  if (obj == NULL)
    return -1;
  
  if (obj->type != t_object)
    return -1;
  
  return BinaryTreeRemove(obj->v.v_object, name);
}

static ssize_t WriteNewline(struct ps_ostream_t *os, ssize_t indent) {
  size_t count;
  
  if (indent < 0)
    return 0;
  
  if (PS_WriteChar(os, '\n') < 0)
    return -1;
  
  for (count = 0; count < indent; count++)
    if (PS_WriteChar(os, ' ') < 0)
      return -1;
  
  return indent + 1;
}

static ssize_t WriteValueIndent(struct ps_ostream_t *os, const struct ps_value_t *v, ssize_t indent) {
  size_t count, bytes;
  ssize_t len;
  struct binary_tree_iterator_t *bti;
  
  if (v == NULL)
    return PS_WriteStr(os, "null");
  
  switch (v->type) {
  case t_null:
    return PS_WriteStr(os, "null");
    
  case t_boolean:
    return PS_WriteStr(os, v->v.v_integer ? "true" : "false");

  case t_integer:
    return PS_Printf(os, "%lld", (long long) v->v.v_integer);

  case t_float:
    return PS_Printf(os, "%.15g", v->v.v_float);

  case t_string:
  case t_variable:
    return PS_WriteJsonStr(os, v->v.v_string, v->type == t_string);

  case t_builtin_func:
    if (PS_WriteStr(os, "builtin<") < 0)
      return -1;
    if (PS_WriteJsonStr(os, v->v.v_string, 0) < 0)
      return -1;
    if (PS_WriteStr(os, ">") < 0)
      return -1;
    return 0;
    
  case t_list:
  case t_function:
    bytes = 0;
    if (PS_WriteChar(os, v->type == t_list ? '[' : '(') < 0)
      return -1;
    bytes++;
    
    for (count = 0; count < v->v.v_list->num_elem; count++) {
      if (count > 0) {
	if (PS_WriteChar(os, ',') < 0)
	  return -1;
	bytes++;
	if ((len = WriteNewline(os, indent)) < 0)
	  return -1;
	bytes += len;
      }
      
      if ((len = WriteValueIndent(os, v->v.v_list->v[count], indent < 0 ? indent : indent + 1)) < 0)
	return -1;
      bytes += len;
    }
    
    if (PS_WriteChar(os, v->type == t_list ? ']' : ')') < 0)
      return -1;
    bytes++;
    return bytes;
    
  case t_object:
    bytes = 0;
    if ((bti = NewBinaryTreeIterator(v->v.v_object)) == NULL)
      return -1;
    if (PS_WriteChar(os, '{') < 0)
      goto objerr;
    bytes++;
    count = 0;
    while (BinaryTreeIteratorNext(bti)) {
      if (count > 0) {
	if (PS_WriteChar(os, ',') < 0)
	  goto objerr;
	bytes++;
	if ((len = WriteNewline(os, indent)) < 0)
	  return -1;
	bytes += len;
      }
      
      if ((len = PS_WriteJsonStr(os, BinaryTreeIteratorKey(bti), 1)) < 0)
	goto objerr;
      bytes += len;
      
      if (PS_WriteChar(os, ':') < 0)
	goto objerr;
      bytes++;

      if (indent > 0) {
	if (PS_WriteChar(os, ' ') < 0)
	  goto objerr;
	bytes++;
      }
      
      if ((len = WriteValueIndent(os, (struct ps_value_t *)BinaryTreeIteratorData(bti), indent < 0 ? indent : indent + len + 3)) < 0)
	goto objerr;
      bytes += len;
      
      count++;
    }
    FreeBinaryTreeIterator(bti);
    if (PS_WriteChar(os, '}') < 0)
      return -1;
    bytes++;
    return 0;
  }
  
  return  0;

 objerr:
  FreeBinaryTreeIterator(bti);
  return -1;
}

ssize_t PS_WriteValue(struct ps_ostream_t *os, const struct ps_value_t *v) {
  return WriteValueIndent(os, v, -1);
}

ssize_t PS_WriteValuePretty(struct ps_ostream_t *os, const struct ps_value_t *v) {
  return WriteValueIndent(os, v, 1);
}

struct ref_func {
  void *ref_data;
  void (*func)(const char *, struct ps_value_t **, void *);
};

static void bt_func(const char *key, void **vv, void *ref_func) {
  struct ps_value_t **val = (struct ps_value_t **) vv;
  struct ref_func *rf = (struct ref_func *) ref_func;

  rf->func(key, val, rf->ref_data);
}

void PS_ValueForeach(const struct ps_value_t *v, void (*func)(const char *, struct ps_value_t **, void *), void *ref_data) {
  struct ref_func rf;
  struct ps_value_t **cur, **end;
  
  if (v == NULL || func == NULL)
    return;
  
  switch (v->type) {
  case t_list:
  case t_function:
    cur = (struct ps_value_t **) v->v.v_list->v;
    end = cur + v->v.v_list->num_elem;
    for (; cur < end; cur++) {
      func(NULL, cur, ref_data);
    }
    break;

  case t_object:
    rf.ref_data = ref_data;
    rf.func = func;
    BinaryTreeForeach(v->v.v_object, bt_func, &rf);
    break;

  default:
    return;
  }
}

struct ps_value_iterator_t {
  struct ps_value_t *v;
  size_t count;
  int init;
  struct binary_tree_iterator_t *bti;
};

struct ps_value_iterator_t *PS_NewValueIterator(const struct ps_value_t *v) {
  struct ps_value_iterator_t *vi;
  
  if (v == NULL)
    goto err;
  
  if ((vi = malloc(sizeof(*vi))) == NULL)
    goto err;
  memset(vi, 0, sizeof(*vi));

  vi->v = (struct ps_value_t *) v;

  switch (v->type) {
  case t_list:
  case t_function:
    break;

  case t_object:
    if ((vi->bti = NewBinaryTreeIterator(v->v.v_object)) == NULL)
      goto err2;
    break;

  default:
    goto err;
  }

  return vi;

 err2:
  free(vi);
 err:
  return NULL;
}

void PS_FreeValueIterator(struct ps_value_iterator_t *vi) {
  if (vi == NULL)
    return;
  
  FreeBinaryTreeIterator(vi->bti);
  free(vi);
}

int PS_ValueIteratorNext(struct ps_value_iterator_t *vi) {
  if (vi->v->type == t_object)
    return BinaryTreeIteratorNext(vi->bti);

  if (vi->init)
    vi->count++;
  vi->init = 1;
  
  return vi->count < vi->v->v.v_list->num_elem;
}

const char *PS_ValueIteratorKey(const struct ps_value_iterator_t *vi) {
  if (vi->v->type == t_object)
    return BinaryTreeIteratorKey(vi->bti);

  return NULL;
}

struct ps_value_t *PS_ValueIteratorData(const struct ps_value_iterator_t *vi) {
  if (vi->v->type == t_object)
    return (struct ps_value_t *)BinaryTreeIteratorData(vi->bti);

  return vi->v->v.v_list->v[vi->count];
}
