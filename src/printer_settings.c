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

#include "printer_settings.h"
#include "ps_path.h"
#include "ps_parse_json.h"
#include "ps_eval.h"
#include "ps_math.h"

struct merge_t {
  struct ps_value_t *target;
  char *forbid;
};

static void MergeMember(const char *key, struct ps_value_t **v, void *ref) {
  struct merge_t *merge = (struct merge_t *) ref;
  struct ps_value_t *mem2, *copy;
  const struct ps_value_t *mem;
  struct merge_t mm;
  
  if (merge->forbid && strcmp(key, merge->forbid) == 0)
    return;
  
  if ((mem = PS_GetMember(merge->target, key, NULL)) == NULL) {
    if ((copy = PS_CopyValue(*v)) == NULL)
      return;
    if (PS_AddMember(merge->target, key, copy) < 0)
      PS_FreeValue(copy);
    return;
  }
  
  if (PS_GetType(mem) != t_object || PS_GetType(*v) != t_object)
    return;
  
  if ((mem2 = PS_CopyValue(mem)) == NULL)
    return;
  
  mm.target = mem2;
  mm.forbid = merge->forbid;
  PS_ValueForeach(*v, MergeMember, &mm);
  PS_AddMember(merge->target, key, mem2);
}

struct index_t {
  struct ps_value_t *set;
  struct ps_value_t *ov;
};

static void BuildIndex(const char *key, struct ps_value_t **data, void *ref) {
  struct index_t *index = (struct index_t *) ref;
  const struct ps_value_t *ovmem, *child;
  struct merge_t merge;
  struct ps_value_t *copy;
  
  if ((ovmem = PS_GetMember(index->ov, key, NULL)))
    copy = PS_CopyValue(ovmem);
  else
    copy = PS_NewObject();

  if (copy == NULL)
    return;
  
  merge.target = copy;
  merge.forbid = "children";
  PS_ValueForeach(*data, MergeMember, &merge);
  
  if ((child = PS_GetMember(*data, "children", NULL)))
    PS_ValueForeach(child, BuildIndex, ref);
  
  if (PS_AddMember(index->set, key, copy) < 0) {
    PS_FreeValue(copy);
    return;
  }
}

static int IndexSettings(struct ps_value_t *pdef) {
  struct ps_value_t *set;
  struct ps_value_t *ov, *ss;
  struct index_t index;
  
  if ((set = PS_NewObject()) == NULL)
    goto err;
  
  ov = PS_GetMember(pdef, "overrides", NULL);

  if ((ss = PS_GetMember(pdef, "settings", NULL)) == NULL) {
    fprintf(stderr, "Cannot find settings entry\n");
    goto err2;
  }

  index.set = set;
  index.ov = ov;
  PS_ValueForeach(ss, BuildIndex, &index);
  
  if (PS_AddMember(pdef, "#set", set) < 0)
    goto err2;

  return 0;

 err2:
  PS_FreeValue(set);
 err:
  return -1;
}

static struct ps_value_t *LoadFileChain(const char *file, const struct ps_value_t *search) {
  struct ps_value_t *final, *pdef, *v, *str;
  struct merge_t merge;
  FILE *in;

  v = NULL;
  final = NULL;
  if ((pdef = PS_NewObject()) == NULL)
    goto err;
  if ((str = PS_NewString(".def.json")) == NULL)
    goto err2;
  
  while (file) {
    if ((in = PS_OpenSearch(file, "r", str, search, &final)) == NULL)
      goto err3;
    PS_FreeValue(v);
    if ((v = PS_ParseJsonFile(in)) == NULL)
      goto err3;
    merge.target = pdef;
    merge.forbid = NULL;
    PS_ValueForeach(v, MergeMember, &merge);
    file = PS_GetString(PS_GetMember(v, "inherits", NULL));
  }
  
  if (IndexSettings(pdef) < 0)
    goto err2;
  
  PS_FreeValue(str);
  PS_FreeValue(final);
  PS_FreeValue(v);
  return pdef;
  
 err3:
  PS_FreeValue(str);
 err2:
  PS_FreeValue(pdef);
 err:
  PS_FreeValue(final);
  PS_FreeValue(v);
  return NULL;
}

struct ext_ref {
  struct ps_value_t *search;
  struct ps_value_t *def;
};

static void LoadExtruder(const char *key, struct ps_value_t **data, void *ref_data) {
  struct ext_ref *ref = (struct ext_ref *) ref_data;
  struct ps_value_t *v;
  
  if ((v = LoadFileChain(PS_GetString(*data), ref->search)) == NULL)
    return;

  if (PS_AddMember(ref->def, key, v) < 0)
    PS_FreeValue(v);
}

static struct ps_value_t *NewDepend(const struct ps_value_t *ps) {
  struct ps_value_t *dep, *v;
  struct ps_value_iterator_t *vi;
  
  if ((dep = PS_NewObject()) == NULL)
    goto err;
  
  if ((vi = PS_NewValueIterator(ps)) == NULL)
    goto err2;
  
  while (PS_ValueIteratorNext(vi)) {
    if ((v = PS_NewObject()) == NULL)
      goto err3;
    if (PS_AddMember(dep, PS_ValueIteratorKey(vi), v) < 0)
      goto err4;
  }

  PS_FreeValueIterator(vi);
  return dep;

 err4:
  PS_FreeValue(v);
 err3:
  PS_FreeValueIterator(vi);
 err2:
  PS_FreeValue(dep);
 err:
  fprintf(stderr, "Error building empty depend\n");
  return NULL;
}

static int AddTriggers(struct ps_value_t *ps, const struct ps_value_t *dep, const char *ext, const char *name) {
  struct ps_value_iterator_t *vi_ext, *vi_set;
  struct ps_value_t *set, *trig, *trig_ext;
  const char *dep_ext, *dep_name;
  
  if ((vi_ext = PS_NewValueIterator(dep)) == NULL)
    goto err;
  
  while (PS_ValueIteratorNext(vi_ext)) {
    dep_ext = PS_ValueIteratorKey(vi_ext);
    
    if ((vi_set = PS_NewValueIterator(PS_ValueIteratorData(vi_ext))) == NULL)
      goto err2;

    while (PS_ValueIteratorNext(vi_set)) {
      dep_name = PS_ValueIteratorKey(vi_set);
      
      if ((set = PS_GetMember(PS_GetMember(PS_GetMember(ps, dep_ext, NULL), "#set", NULL), dep_name, NULL)) == NULL) {
	fprintf(stderr, "Warning: Unknown dependancy %s->%s\n", dep_ext, dep_name);
	continue;
      }
      
      if ((trig = PS_GetMember(set, "#trigger", NULL)) == NULL) {
	if ((trig = PS_NewObject()) == NULL)
	  goto err3;

	if (PS_AddMember(set, "#trigger", trig) < 0) {
	  PS_FreeValue(trig);
	  goto err3;
	}
      }

      if ((trig_ext = PS_GetMember(trig, ext, NULL)) == NULL) {
	if ((trig_ext = PS_NewObject()) == NULL)
	  goto err3;

	if (PS_AddMember(trig, ext, trig_ext) < 0) {
	  PS_FreeValue(trig_ext);
	  goto err3;
	}
      }
      
      if (PS_AddMember(trig_ext, name, PS_NewBoolean(1)) < 0)
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

static int BuildDeps(struct ps_value_t *ps) {
  struct ps_value_iterator_t *vi_ext, *vi_set;
  struct ps_value_t *set, *v, *expr, *dep;
  const char *ext;
  
  if ((vi_ext = PS_NewValueIterator(ps)) == NULL)
    goto err;
  
  while (PS_ValueIteratorNext(vi_ext)) {
    ext = PS_ValueIteratorKey(vi_ext);
    
    if ((vi_set = PS_NewValueIterator(PS_GetMember(PS_ValueIteratorData(vi_ext), "#set", NULL))) == NULL)
      goto err2;

    while (PS_ValueIteratorNext(vi_set)) {
      set = PS_ValueIteratorData(vi_set);
      
      if ((v = PS_GetMember(set, "value", NULL)) == NULL)
	continue;
      
      if ((dep = NewDepend(ps)) == NULL)
	goto err3;
      
      if ((expr = PS_ParseForEval(v, ext, dep)) == NULL) {
	fprintf(stderr, "Error parsing for eval '%s'\n", PS_GetString(v));
	goto err4;
      }
      
      if (PS_AddMember(set, "#eval", expr) < 0)
	goto err5;
      
      if (PS_AddMember(set, "#dep", dep) < 0)
	goto err4;

      if (AddTriggers(ps, dep, ext, PS_ValueIteratorKey(vi_set)) < 0)
	goto err3;
    }
    
    PS_FreeValueIterator(vi_set);
  }

  PS_FreeValueIterator(vi_ext);
  return 0;

 err5:
  PS_FreeValue(expr);
 err4:
  PS_FreeValue(dep);
 err3:
  PS_FreeValueIterator(vi_set);
 err2:
  PS_FreeValueIterator(vi_ext);
 err:
  return -1;
}

struct ps_value_t *PS_New(const char *printer, const struct ps_value_t *search) {
  struct ps_value_t *ps;
  struct ps_value_t *v;
  size_t num_extr;
  struct ps_value_t *c;
  struct ext_ref ref;

  if ((ps = PS_NewObject()) == NULL)
    goto err;
  
  if ((v = LoadFileChain(printer, search)) == NULL)
    goto err2;
  if (PS_AddMember(ps, "#global", v) < 0)
    goto err3;
  
  if ((c = PS_GetMember(PS_GetMember(v, "metadata", NULL), "machine_extruder_trains", NULL)) == NULL) {
    fprintf(stderr, "Could not find metadata -> machine_extruder_trains in printer definition\n");
    goto err2;
  }
  if (PS_GetType(c) != t_object) {
    fprintf(stderr, "metadata -> machine_extruder_trains must be an object\n");
    goto err2;
  }
  
  num_extr = PS_ItemCount(c);
  if (num_extr == 0) {
    fprintf(stderr, "At least one extruder is required\n");
    goto err2;
  }

  ref.search = (struct ps_value_t *) search;
  ref.def = ps;
  PS_ValueForeach(c, LoadExtruder, &ref);
  
  if (BuildDeps(ps) < 0)
    goto err2;
  
  if ((v = PS_NewString(printer)) == NULL)
    goto err2;

  if (PS_AddMember(PS_GetMember(ps, "#global", NULL), "#filename", v) < 0)
    goto err3;

  if ((v = PS_CopyValue(search)) == NULL)
    goto err3;
  
  if (PS_AddMember(PS_GetMember(ps, "#global", NULL), "#search", v) < 0)
    goto err3;
  
  return ps;

 err3:
  PS_FreeValue(v);
 err2:
  PS_FreeValue(ps);
 err:
  return NULL;
}

const char *PS_GetPrinter(const struct ps_value_t *ps) {
  return PS_GetString(PS_GetMember(PS_GetMember(ps, "#global", NULL), "#filename", NULL));
}

const struct ps_value_t *PS_GetSearch(const struct ps_value_t *ps) {
  return PS_GetMember(PS_GetMember(ps, "#global", NULL), "#search", NULL);
}

struct ps_value_t *PS_ListExtruders(const struct ps_value_t *ps) {
  struct ps_value_t *ext, *v;
  struct ps_value_iterator_t *vi;
  
  if ((ext = PS_NewList()) == NULL)
    goto err;
  
  if ((vi = PS_NewValueIterator(ps)) == NULL)
    goto err2;
  
  while (PS_ValueIteratorNext(vi)) {
    if ((v = PS_NewString(PS_ValueIteratorKey(vi))) == NULL)
      goto err3;
    if (PS_AppendToList(ext, v) < 0)
      goto err4;
  }

  PS_FreeValueIterator(vi);
  return ext;

 err4:
  PS_FreeValue(v);
 err3:
  PS_FreeValueIterator(vi);
 err2:
  PS_FreeValue(ext);
 err:
  fprintf(stderr, "Error building extruder list\n");
  return NULL;
}

struct ps_value_t *PS_GetDefaults(const struct ps_value_t *ps) {
  struct ps_value_t *c, *d;
  struct ps_value_t *g, *v, *dd;
  struct ps_value_iterator_t *ex, *vi;

  if ((g = PS_NewObject()) == NULL)
    goto err;
  
  if ((ex = PS_NewValueIterator(ps)) == NULL)
    goto err2;

  while (PS_ValueIteratorNext(ex)) {
    if ((c = PS_GetMember(PS_ValueIteratorData(ex), "#set", NULL)) == NULL)
      goto err3;
    
    if ((v = PS_NewObject()) == NULL)
      goto err3;
    
    if ((vi = PS_NewValueIterator(c)) == NULL)
      goto err4;
    
    while (PS_ValueIteratorNext(vi)) {
      if ((d = PS_GetMember(PS_ValueIteratorData(vi), "default_value", NULL)) == NULL)
	continue;
      if ((dd = PS_CopyValue(d)) == NULL)
	goto err5;
      if (PS_AddMember(v, PS_ValueIteratorKey(vi), dd) < 0)
	PS_FreeValue(dd);
    }
    PS_FreeValueIterator(vi);

    if (PS_AddMember(g, PS_ValueIteratorKey(ex), v) < 0)
      goto err4;
  }

  PS_FreeValueIterator(ex);
  
  return g;
  
 err5:
  PS_FreeValueIterator(vi);
 err4:
  PS_FreeValue(v);
 err3:
  PS_FreeValueIterator(ex);
 err2:
  PS_FreeValue(g);
 err:
  return NULL;
}

const struct ps_value_t *PS_GetSettingProperties(const struct ps_value_t *ps, const char *extruder, const char *setting) {
  return PS_GetMember(PS_GetMember(PS_GetMember(ps, extruder, NULL), "#set", NULL), setting, NULL);
}

struct ps_value_t *PS_BlankSettings(const struct ps_value_t *ps) {
  struct ps_value_t *obj, *v;
  struct ps_value_iterator_t *vi;

  if ((obj = PS_NewObject()) == NULL)
    goto err;
  
  if ((vi = PS_NewValueIterator(ps)) == NULL)
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

int PS_AddSetting(struct ps_value_t *set, const char *ext, const char *name, const struct ps_value_t *value) {
  struct ps_value_t *v;
  
  if (ext == NULL)
    ext = "#global";
  
  if ((v = PS_CopyValue(value)) == NULL)
    goto err;

  if (PS_AddMember(PS_GetMember(set, ext, NULL), name, v) < 0)
    goto err2;
  
  return 0;
  
 err2:
  PS_FreeValue(v);
 err:
  return -1;
}

int PS_MergeSettings(struct ps_value_t *dest, struct ps_value_t *src) {
  struct ps_value_t *set, *v;
  struct ps_value_iterator_t *vi_ext, *vi_set;
  const char *ext, *name;
  
  if ((vi_ext = PS_NewValueIterator(src)) == NULL)
    goto err;

  while (PS_ValueIteratorNext(vi_ext)) {
    ext = PS_ValueIteratorKey(vi_ext);
    
    if ((vi_set = PS_NewValueIterator(PS_ValueIteratorData(vi_ext))) == NULL)
      goto err2;
    
    while (PS_ValueIteratorNext(vi_set)) {
      name = PS_ValueIteratorKey(vi_set);
      set = PS_ValueIteratorData(vi_set);
      
      if ((v = PS_CopyValue(set)) == NULL)
	goto err3;
      
      if (PS_AddMember(PS_GetMember(dest, ext, NULL), name, v) < 0)
	goto err4;
    }

    PS_FreeValueIterator(vi_set);
  }
  
  PS_FreeValueIterator(vi_ext);
  return 0;
  
 err4:
  PS_FreeValue(v);
 err3:
  PS_FreeValueIterator(vi_set);
 err2:
  PS_FreeValueIterator(vi_ext);
 err:
  return -1;
}

struct queue_t {
  char *ext;
  char *name;
  struct queue_t *next;
};

static int Enqueue(struct queue_t ***tail, struct ps_value_t *members, const char *ext, const char *name) {
  struct queue_t *q;
#ifdef DEBUG
  printf("Adding to eval queue %s->%s\n", ext, name);
#endif
  
  if (PS_GetMember(PS_GetMember(members, ext, NULL), name, NULL))
    return 0;
  
  if ((q = malloc(sizeof(*q))) == NULL)
    goto err;
  memset(q, 0, sizeof(*q));

  q->ext = (char *) ext;
  q->name = (char *) name;
  q->next = NULL;
  
  if (PS_AddMember(PS_GetMember(members, ext, NULL), name, PS_NewBoolean(1)) < 0)
    goto err2;
  
  **tail = q;
  *tail = &q->next;
  return 0;
  
 err2:
  free(q);
 err:
  return -1;
}

static void Dequeue(struct queue_t **queue, struct ps_value_t *members, const char **ext, const char **name) {
  struct queue_t *q;
  
  if (*queue == NULL)
    return;

  *ext = (*queue)->ext;
  *name = (*queue)->name;
  
  PS_RemoveMember(PS_GetMember(members, *ext, NULL), *name);
  
  q = *queue;
  *queue = q->next;

#ifdef DEBUG
  printf("Preparing to eval %s->%s\n", *ext, *name);
#endif
  
  free(q);
}

static void FreeQueue(struct queue_t *queue) {
  struct queue_t *next;

  while (queue) {
    next = queue->next;
    free(queue);
    queue = next;
  }
}

static int CheckType(const struct ps_value_t *type, const struct ps_value_t *val) {
  const char *str;
  enum ps_type_t vtype;
  
  if (type == NULL || PS_GetType(type) != t_string)
    return 0;
  
  if ((str = PS_GetString(type)) == NULL)
    return 0;
  
  vtype = PS_GetType(val);
  
  if (strcmp(str, "str") == 0 ||
      strcmp(str, "enum") == 0 ||
      strcmp(str, "extruder") == 0 ||
      strcmp(str, "optional_extruder") == 0)
    return vtype == t_string ? 0 : -1;
    
  if (strcmp(str, "bool") == 0)
    return vtype == t_boolean ? 0 : -1;
  
  if (strcmp(str, "float") == 0 || strcmp(str, "int") == 0)
    return (vtype == t_float || vtype == t_integer) ? 0 : -1;
  
  if (str[0] == '[' || strcmp(str, "polygons") == 0)
    return vtype == t_list ? 0 : -1;
  
  return 0;
}

static int EvalCtx(const struct ps_value_t *ps, struct ps_context_t *ctx) {
  struct queue_t *queue, **tail;
  struct ps_value_t *members, *result, *set, *dflt, *trig;
  struct ps_value_iterator_t *vi_ext;
  struct ps_value_iterator_t *vi_set;
  const char *ext, *name;
  int count;

  queue = NULL;
  tail = &queue;
  if ((members = PS_BlankSettings(ps)) == NULL)
    goto err;
  
  if ((vi_ext = PS_NewValueIterator(ps)) == NULL)
    goto err2;
  
  while (PS_ValueIteratorNext(vi_ext)) {
    ext = PS_ValueIteratorKey(vi_ext);
    
    if ((vi_set = PS_NewValueIterator(PS_GetMember(PS_ValueIteratorData(vi_ext), "#set", NULL))) == NULL)
      goto err3;

    while (PS_ValueIteratorNext(vi_set)) {
      set = PS_ValueIteratorData(vi_set);
      name = PS_ValueIteratorKey(vi_set);

      if (!PS_GetMember(set, "#eval", NULL))
	continue;
      
      if (PS_CtxIsHard(ctx, ext, name))
	continue;
      
      if (Enqueue(&tail, members, ext, name) < 0)
	goto err4;
    }
    
    PS_FreeValueIterator(vi_set);
  }
  
  PS_FreeValueIterator(vi_ext);

  count = 0;
  while (queue) {
    if (count++ >= 100000) {
      fprintf(stderr, "Maximum number of evals exceeded.  Possible circular references.\n");
      goto err2;
    }
    Dequeue(&queue, members, &ext, &name);

    PS_CtxPush(ctx, ext);
    set = PS_GetMember(PS_GetMember(PS_GetMember(ps, ext, NULL), "#set", NULL), name, NULL);
    if ((result = PS_Eval(PS_GetMember(set, "#eval", NULL), ctx)) == NULL) {
      PS_CtxPop(ctx);
      fprintf(stderr, "Unable to evaluate expression for %s->%s\n", ext, name);
      continue;
    }
    PS_CtxPop(ctx);
    
    dflt = PS_GetMember(set, "default_value", NULL);
    if (dflt && PS_AsBoolean(PS_Call2(PS_EQ, result, dflt))) {
      PS_FreeValue(result);
      result = NULL;
    } else if (CheckType(PS_GetMember(set, "type", NULL), result) < 0) {
      fprintf(stderr, "Invalid type for %s->%s\n", ext, name);
      result = NULL;
    }
    
    if (PS_CtxAddValue(ctx, ext, name, result) < 0) {
      PS_FreeValue(result);
      goto err2;
    }

    if ((trig = PS_GetMember(set, "#trigger", NULL)) == NULL)
      continue;
    
    if ((vi_ext = PS_NewValueIterator(trig)) == NULL)
      goto err2;
    
    while (PS_ValueIteratorNext(vi_ext)) {
      ext = PS_ValueIteratorKey(vi_ext);
      
      if ((vi_set = PS_NewValueIterator(PS_ValueIteratorData(vi_ext))) == NULL)
	goto err3;
      
      while (PS_ValueIteratorNext(vi_set)) {
	if (Enqueue(&tail, members, ext, PS_ValueIteratorKey(vi_set)) < 0)
	  goto err4;
      }
      
      PS_FreeValueIterator(vi_set);
    }
    
    PS_FreeValueIterator(vi_ext);
  }
  
  return 0;
    
 err4:
  PS_FreeValueIterator(vi_set);
 err3:
  PS_FreeValueIterator(vi_ext);
 err2:
  FreeQueue(queue);
  PS_FreeValue(members);
 err:
  return -1;
}

struct ps_value_t *PS_EvalAll(const struct ps_value_t *ps, const struct ps_value_t *settings) {
  struct ps_value_t *dflt;
  struct ps_context_t *ctx;
  struct ps_value_t *eval;
  
  if ((dflt = PS_GetDefaults(ps)) == NULL)
    goto err;
  
  if ((ctx = PS_NewCtx(settings, dflt)) == NULL)
    goto err2;
  
  if (EvalCtx(ps, ctx) < 0)
    goto err3;
  
  if ((eval = PS_CopyValue(PS_CtxGetValues(ctx))) == NULL)
    goto err3;
  
  PS_FreeCtx(ctx);
  PS_FreeValue(dflt);
  return eval;

 err3:
  PS_FreeCtx(ctx);
 err2:
  PS_FreeValue(dflt);
 err:
  return NULL;
}
