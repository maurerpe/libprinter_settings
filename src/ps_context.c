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

#include <math.h>
#include <string.h>

#include "ps_context.h"
#include "binary_tree.h"

struct list_t {
  char *ext;
  struct list_t *next;
};

struct ps_context_t {
  struct ps_value_t *hard;
  struct ps_value_t *over;
  struct ps_value_t *dflt;
  struct ps_value_t *const_val;
  
  struct list_t *ext_stack;
};

int PS_CtxIsConstant(const char *name) {
  return strcmp(name, "math.pi") == 0;
}

static struct ps_value_t *BuildConst(void) {
  struct ps_value_t *cv, *v;

  if ((cv = PS_NewObject()) == NULL)
    goto err;

  if ((v = PS_NewFloat(M_PI)) == NULL)
    goto err2;
  if (PS_AddMember(cv, "math.pi", v) < 0)
    goto err3;
  
  return cv;
  
 err3:
  PS_FreeValue(v);
 err2:
  PS_FreeValue(cv);
 err:
  return NULL;
}

static struct list_t *NewList(const char *ext) {
  struct list_t *list;

  if ((list = malloc(sizeof(*list))) == NULL)
    goto err;
  memset(list, 0, sizeof(*list));
  
  if ((list->ext = strdup(ext)) == NULL)
    goto err2;
  
  return list;
  
 err2:
  free(list);
 err:
  return NULL;
}

static void FreeList(struct list_t *list) {
  if (list == NULL)
    return;
  
  free(list->ext);
  free(list);
}

static struct ps_value_t *BlankExtObjFromTemplate(struct ps_value_t *template) {
  struct ps_value_t *obj, *v;
  struct ps_value_iterator_t *vi;

  if ((obj = PS_NewObject()) == NULL)
    goto err;
  
  if ((vi = PS_NewValueIterator(template)) == NULL)
    goto err2;

  while (PS_ValueIteratorNext(vi)) {
    if ((v = PS_NewObject()) == NULL)
      goto err3;

    if (PS_AddMember(obj, PS_ValueIteratorKey(vi), v) < 0)
      goto err4;
  }
  
  PS_FreeValueIterator(vi);
  return obj;
  
 err4:
  PS_FreeValue(v);
 err3:
  PS_FreeValueIterator(vi);
 err2:
  PS_FreeValue(obj);
 err:
  return NULL;
}

static const char *GetFirstExt(struct ps_value_t *template) {
  const char *ext;
  struct ps_value_iterator_t *vi;

  if ((vi = PS_NewValueIterator(template)) == NULL)
    return NULL;

  if (!PS_ValueIteratorNext(vi))
    return NULL;
  
  ext = PS_ValueIteratorKey(vi);
  PS_FreeValueIterator(vi);
  return ext;
}

static int MarkHard(struct ps_value_t *hard, const struct ps_value_t *hard_settings) {
  struct ps_value_iterator_t *vi_ext, *vi_set;
  const char *ext, *name;
  
  if ((vi_ext = PS_NewValueIterator(hard_settings)) == NULL)
    goto err;
  
  while (PS_ValueIteratorNext(vi_ext)) {
    ext = PS_ValueIteratorKey(vi_ext);
    
    if ((vi_set = PS_NewValueIterator(PS_ValueIteratorData(vi_ext))) == NULL)
      goto err2;
    
    while (PS_ValueIteratorNext(vi_set)) {
      name = PS_ValueIteratorKey(vi_set);
      
      if (PS_AddMember(PS_GetMember(hard, ext, NULL), name, PS_NewBoolean(1)) < 0)
	goto err3;
    }

    PS_FreeValueIterator(vi_set);
  }
  
  PS_FreeValueIterator(vi_ext);
  return 0;
  
 err3:
  PS_FreeValueIterator(vi_set);
 err2:
  PS_FreeValueIterator(vi_ext);
 err:
  return -1;
}

struct ps_context_t *PS_NewCtx(const struct ps_value_t *hard_settings, const struct ps_value_t *dflt) {
  struct ps_context_t *ctx;
  const char *ext;
  
  if ((ctx = malloc(sizeof(*ctx))) == NULL)
    goto err;
  memset(ctx, 0, sizeof(*ctx));
  
  if ((ctx->dflt = PS_CopyValue(dflt)) == NULL)
    goto err2;
  
  if ((ctx->hard = BlankExtObjFromTemplate(ctx->dflt)) == NULL)
    goto err3;
  
  if (hard_settings && MarkHard(ctx->hard, hard_settings) < 0)
    goto err4;
  
  if ((ctx->over = PS_CopyValue(hard_settings ? hard_settings : ctx->hard)) == NULL)
    goto err4;
  
  if ((ctx->const_val = BuildConst()) == NULL)
    goto err5;
  
  if ((ext = GetFirstExt(ctx->dflt)) == NULL)
    goto err6;
  
  if ((ctx->ext_stack = NewList(ext)) == NULL)
    goto err6;
  
  return ctx;

 err6:
  PS_FreeValue(ctx->const_val);
 err5:
  PS_FreeValue(ctx->over);
 err4:
  PS_FreeValue(ctx->hard);
 err3:
  PS_FreeValue(ctx->dflt);
 err2:
  free(ctx);
 err:
  fprintf(stderr, "Error: Could not create constant value object\n");
  return NULL;
}

void PS_FreeCtx(struct ps_context_t *ctx) {
  struct list_t *cur, *next;

  if (ctx == NULL)
    return;
  
  cur = ctx->ext_stack;
  while (cur) {
    next = cur->next;
    FreeList(cur);
    cur = next;
  }
  
  PS_FreeValue(ctx->const_val);
  PS_FreeValue(ctx->over);
  PS_FreeValue(ctx->hard);
  PS_FreeValue(ctx->dflt);
  free(ctx);
}

struct ps_value_t *PS_BlankExtObj(struct ps_context_t *ctx) {
  return BlankExtObjFromTemplate(ctx->dflt);
}

const struct ps_value_t *PS_CtxGetValues(struct ps_context_t *ctx) {
  return ctx->over;
}

int PS_CtxIsHard(struct ps_context_t *ctx, const char *ext, const char *name) {
  return PS_GetMember(PS_GetMember(ctx->hard, ext, NULL), name, NULL) != NULL;
}

int PS_CtxAddValue(struct ps_context_t *ctx, const char *ext, const char *name, struct ps_value_t *v) {
  if (PS_GetMember(PS_GetMember(ctx->hard, ext, NULL), name, NULL))
    return 0;
  
  if (!PS_GetMember(PS_GetMember(ctx->dflt, ext, NULL), name, NULL))
    fprintf(stderr, "Warning: Adding setting without default value, possible typo %s->%s\n", ext, name);

  if (!v)
    return PS_RemoveMember(PS_GetMember(ctx->over, ext, NULL), name);
  
  return PS_AddMember(PS_GetMember(ctx->over, ext, NULL), name, v);
}

static const struct ps_value_t *RawLookup(struct ps_context_t *ctx, const char *ext, const char *name, int quiet) {
  const struct ps_value_t *v;
  if ((v = PS_GetMember(PS_GetMember(ctx->over, ext, NULL), name, NULL)))
    return v;

  if ((v = PS_GetMember(PS_GetMember(ctx->dflt, ext, NULL), name, NULL)))
    return v;
  
  if (strcmp(ext, "#global") != 0 && (v = RawLookup(ctx, "#global", name, 1)))
    return v;
  
  if ((v = PS_GetMember(ctx->const_val, name, NULL)))
    return v;
  
  if (!quiet)
    fprintf(stderr, "Unknown setting %s->%s\n", ext, name);
  return NULL;
}

const struct ps_value_t *PS_CtxLookup(struct ps_context_t *ctx, const char *name) {
  if (ctx->ext_stack == NULL)
    return NULL;
  
  return RawLookup(ctx, ctx->ext_stack->ext, name, 0);
}

struct ps_value_t *PS_CtxLookupAll(struct ps_context_t *ctx, const char *name) {
  struct ps_value_t *list, *v;
  struct ps_value_iterator_t *vi;
  
  if ((list = PS_NewList()) == NULL)
    goto err;
  
  if ((vi = PS_NewValueIterator(ctx->dflt)) == NULL)
    goto err2;

  if (!PS_ValueIteratorNext(vi))
    goto err3;
  
  while (PS_ValueIteratorNext(vi)) {
    if ((v = PS_AddRef(RawLookup(ctx, PS_ValueIteratorKey(vi), name, 0))) == NULL)
      goto err3;

    if (PS_AppendToList(list, v) < 0)
      goto err4;
  }

  PS_FreeValueIterator(vi);
  return list;
  
 err4:
  PS_FreeValue(v);
 err3:
  PS_FreeValueIterator(vi);
 err2:
  PS_FreeValue(list);
 err:
  return NULL;
}

int PS_CtxPush(struct ps_context_t *ctx, const char *ext) {
  struct list_t *list;

  if ((list = NewList(ext)) == NULL)
    goto err;

  list->next = ctx->ext_stack;
  ctx->ext_stack = list;
  
  return 0;
  
 err:
  return -1;
}

void PS_CtxPop(struct ps_context_t *ctx) {
  struct list_t *list;
  
  if ((list = ctx->ext_stack) == NULL) {
    fprintf(stderr, "Internal error: Popping from empty extruder stack\n");
    return;
  }
  
  ctx->ext_stack = list->next;
  FreeList(list);
}
