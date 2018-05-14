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
#include <unistd.h>

#include <math.h>
#include <string.h>

#include "ps_math.h"
#include "ps_eval.h"

struct ps_value_t *PS_Call1(const ps_func_t func, const struct ps_value_t *v1) {
  struct ps_value_t *list, *cp, *ret;

  if ((list = PS_NewList()) == NULL)
    goto err;

  if ((cp = PS_AddRef(v1)) == NULL)
    goto err2;
  
  if (PS_AppendToList(list, cp) < 0)
    goto err3;
  
  ret = func(list);
  PS_FreeValue(list);

  return ret;
  
 err3:
  PS_FreeValue(cp);
 err2:
  PS_FreeValue(list);
 err:
  return NULL;
}

struct ps_value_t *PS_Call2(const ps_func_t func, const struct ps_value_t *v1, const struct ps_value_t *v2) {
  struct ps_value_t *list, *cp, *ret;

  if ((list = PS_NewList()) == NULL)
    goto err;

  if ((cp = PS_AddRef(v1)) == NULL)
    goto err2;
  
  if (PS_AppendToList(list, cp) < 0)
    goto err3;
  
  if ((cp = PS_AddRef(v2)) == NULL)
    goto err2;
  
  if (PS_AppendToList(list, cp) < 0)
    goto err3;
  
  ret = func(list);
  PS_FreeValue(list);

  return ret;
  
 err3:
  PS_FreeValue(cp);
 err2:
  PS_FreeValue(list);
 err:
  return NULL;
}

static ssize_t VerifyArgs(const struct ps_value_t *v, size_t min, size_t max, enum ps_type_t *type) {
  size_t len, count;
  enum ps_type_t tt;
  
  if (v == NULL || PS_GetType(v) != t_list) {
    fprintf(stderr, "Internal error: v must be list of args\n");
    return -1;
  }
  
  len = PS_ItemCount(v);
  if (min > len || len > max) {
    fprintf(stderr, "Incorrect number of args: expected [%zu, %zu], found %zu\n", min, max, len);
    return -1;
  }

  if (type) {
    if (len == 0)
      *type = t_integer;
    else {
      *type = PS_GetType(PS_GetItem(v, 0));
      for (count = 1; count < len; count++) {
	tt = PS_GetType(PS_GetItem(v, count));
	if (tt == t_null ||
	    (*type == t_string && tt >= t_boolean) ||
	    (*type != t_string && tt > *type))
	  *type = tt;
      }
    }
  }
  
  return len;
}

#define IDENTITY1 PS_CopyValue(PS_GetItem(v, 0))
#define F1 PS_AsFloat(PS_GetItem(v, 0))
#define F2 PS_AsFloat(PS_GetItem(v, 1))
#define CALL1(func, modfunc) (func(modfunc(PS_GetItem(v, 0))))
#define CALL2(func, modfunc) (func(modfunc(PS_GetItem(v, 0)),modfunc(PS_GetItem(v, 1))))
#define INFIX(op, modfunc) ((modfunc(PS_GetItem(v, 0))) op (modfunc(PS_GetItem(v, 1))))

#define NUM_FUNC(name, intr, floatr)			\
  struct ps_value_t * name (const struct ps_value_t *v) {	\
    enum ps_type_t type;				\
    							\
    if (VerifyArgs(v, 2, 2, &type) < 0)			\
      return NULL;					\
    							\
    switch (type) {					\
    case t_boolean:					\
    case t_integer:					\
      return intr;					\
      							\
    case t_float:					\
      return floatr;					\
      							\
    default:						\
      fprintf(stderr, "Wrong type args to function " #name "\n");	\
      return NULL;					\
    }							\
  }							\

#define SQRT_INT64_MAX 3037000448

static struct ps_value_t *IntExpt(int64_t base, int64_t exp) {
  int64_t val, ee, bit;
  
  if (exp < 0)
    goto err;

  for (bit = 0, ee = exp; ee != 0; bit++, ee >>= 1)
    ;

  val = 1;
  while (bit > 0) {
    if (llabs(val) > SQRT_INT64_MAX)
      goto err;
    val *= val;
    
    if ((exp >> (--bit)) & 0x1) {
      if (llabs(val) > INT64_MAX / llabs(base))
	goto err;
      val *= base;
    }
  }

  return PS_NewInteger(val);

 err:
  return PS_NewFloat(pow(base, exp));
}

NUM_FUNC(PS_Expt,
	 CALL2(IntExpt, PS_AsInteger),
	 PS_NewFloat(CALL2(pow, PS_AsFloat)))

static struct ps_value_t *IntMul(int64_t a, int64_t b) {
  if (abs(a) > INT64_MAX / abs(b))
    return PS_NewFloat(((double) a) * ((double) b));

  return PS_NewInteger(a * b);
}

NUM_FUNC(PS_Mul,
	 CALL2(IntMul, PS_AsInteger),
	 PS_NewFloat(INFIX(*, PS_AsFloat)))

static struct ps_value_t *IntDiv(int64_t a, int64_t b) {
  if (a % b != 0)
    return PS_NewFloat(((double) a) * ((double) b));

  return PS_NewInteger(a % b);
}

NUM_FUNC(PS_Div,
	 CALL2(IntDiv, PS_AsInteger),
	 PS_NewFloat(INFIX(/, PS_AsFloat)))

static struct ps_value_t *Concat(const struct ps_value_t *v) {
  struct ps_value_t *concat;
  struct ps_ostream_t *os;
  
  if ((os = PS_NewStrOStream()) == NULL)
    goto err;

  if (PS_GetType(PS_GetItem(v, 0)) == t_string) {
    if (PS_WriteStr(os, PS_GetString(PS_GetItem(v, 0))) < 0)
      goto err2;
  } else {
    if (PS_WriteValue(os, PS_GetItem(v, 0)) < 0)
      goto err2;
  }

  if (PS_GetType(PS_GetItem(v, 1)) == t_string) {
    if (PS_WriteStr(os, PS_GetString(PS_GetItem(v, 1))) < 0)
      goto err2;
  } else {
    if (PS_WriteValue(os, PS_GetItem(v, 1)) < 0)
      goto err2;
  }
  
  if ((concat = PS_NewString(PS_OStreamContents(os))) == NULL)
    goto err2;

  PS_FreeOStream(os);

  return concat;
  
 err2:
  PS_FreeOStream(os);
 err:
  return NULL;
}

struct ps_value_t *PS_Add(const struct ps_value_t *v) {
  enum ps_type_t type;
  ssize_t len;
  
  if ((len = VerifyArgs(v, 1, 2, &type)) < 0)
    return NULL;
  
  if (len == 2 && type <= t_string &&
      (PS_GetType(PS_GetItem(v, 0)) == t_string ||
       PS_GetType(PS_GetItem(v, 1)) == t_string)) {
    return Concat(v);
  }
  
  switch (type) {
  case t_boolean:
  case t_integer:
    if (len == 1)
      return PS_NewInteger(PS_AsInteger(PS_GetItem(v, 0)));
    
    return PS_NewInteger(INFIX(+, PS_AsInteger));
    
  case t_float:
    if (len == 1)
      return PS_NewFloat(PS_AsFloat(PS_GetItem(v, 0)));
    
    return PS_NewFloat(INFIX(+, PS_AsFloat));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Add\n");
      return NULL;
  }
}

struct ps_value_t *PS_Sub(const struct ps_value_t *v) {
  enum ps_type_t type;
  ssize_t len;
  
  if ((len = VerifyArgs(v, 1, 2, &type)) < 0)
    return NULL;
  
  switch (type) {
  case t_boolean:
  case t_integer:
    if (len == 1)
      return PS_NewInteger(-PS_AsInteger(PS_GetItem(v, 0)));
    
    return PS_NewInteger(INFIX(-, PS_AsInteger));
    
  case t_float:
    if (len == 1)
      return PS_NewFloat(-PS_AsFloat(PS_GetItem(v, 0)));
    
    return PS_NewFloat(INFIX(-, PS_AsFloat));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Sub\n");
      return NULL;
  }
}

#define CMP_FUNC(name, cmp)				\
  struct ps_value_t * name (const struct ps_value_t *v) {	\
    enum ps_type_t type;				\
    							\
    if (VerifyArgs(v, 2, 2, &type) < 0)			\
      return NULL;					\
    							\
    switch (type) {					\
    case t_boolean:					\
      return PS_NewBoolean(INFIX(cmp, PS_AsBoolean));	\
      							\
    case t_integer:					\
      return PS_NewBoolean(INFIX(cmp, PS_AsInteger));	\
      							\
    case t_float:					\
      return PS_NewBoolean(INFIX(cmp, PS_AsFloat));	\
      							\
    case t_string:					\
      return PS_NewBoolean(CALL2(strcmp, PS_GetString) cmp 0);	\
      							\
    default:						\
      fprintf(stderr, "Wrong type args to function " #name "\n");	\
      return NULL;					\
    }							\
  }							\

CMP_FUNC(PS_LT, <)
CMP_FUNC(PS_GT, >)
CMP_FUNC(PS_LE, <=)
CMP_FUNC(PS_GE, >=)

static int PS_EqRaw(const struct ps_value_t *v);

static int PS_EqCall(const struct ps_value_t *va, const struct ps_value_t *vb) {
  struct ps_value_t *list, *c;
  int ret;
  
  if ((list = PS_NewList()) == NULL)
    goto err;
  
  if ((c = PS_AddRef(va)) == NULL)
    goto err2;    
  if (PS_AppendToList(list, c) < 0)
    goto err3;
  
  if ((c = PS_AddRef(vb)) == NULL)
    goto err2;
  if (PS_AppendToList(list, c) < 0)
    goto err3;
  
  ret = PS_EqRaw(list);
  PS_FreeValue(list);
  
  return ret;

 err3:
  PS_FreeValue(c);
 err2:
  PS_FreeValue(list);
 err:
  return -1;
}

static int PS_EqList(const struct ps_value_t *va, const struct ps_value_t *vb) {
  size_t count, len;
  int ret;
  
  len = PS_ItemCount(va);
  for (count = 0; count < len; count++) {
    ret = PS_EqCall(PS_GetItem(va, count), PS_GetItem(vb, count));
    
    if (ret < 1)
      return ret;
  }
  
  return 1;
}

static int PS_EqObject(const struct ps_value_t *va, const struct ps_value_t *vb) {
  struct ps_value_iterator_t *via, *vib;
  int ret = -1;

  if ((via = PS_NewValueIterator(va)) == NULL)
    goto err;

  if ((vib = PS_NewValueIterator(vb)) == NULL)
    goto err2;

  while (PS_ValueIteratorNext(via) && PS_ValueIteratorNext(vib)) {
    ret = PS_EqCall(PS_ValueIteratorData(via), PS_ValueIteratorData(vib));
    
    if (ret < 1)
      goto err3;
  }

  ret = 1;
  /* Fall through */

 err3:
  PS_FreeValueIterator(vib);
 err2:
  PS_FreeValueIterator(via);
 err:
  return ret;
}

static int PS_EqRaw(const struct ps_value_t *v) {
  enum ps_type_t type;
  
  if (VerifyArgs(v, 2, 2, &type) < 0)
    return -1;
  
  switch (type) {
  case t_null:
    return INFIX(==, PS_GetType);
    
  case t_boolean:
    return INFIX(==, PS_AsBoolean);
    
  case t_integer:
    return INFIX(==, PS_AsInteger);
    
  case t_float:
    return INFIX(==, PS_AsFloat);
    
  case t_string:
    return CALL2(strcmp, PS_GetString) == 0;
    
  case t_variable:
    return INFIX(==, PS_GetType) && CALL2(strcmp, PS_GetString) == 0;

  case t_list:
  case t_function:
    return INFIX(==, PS_GetType) && INFIX(==, PS_ItemCount) && PS_EqList(PS_GetItem(v, 0), PS_GetItem(v, 1));

  case t_object:
    return INFIX(==, PS_GetType) && INFIX(==, PS_ItemCount) && PS_EqObject(PS_GetItem(v, 0), PS_GetItem(v, 1));
    
  default:
    fprintf(stderr, "Wrong type args to function PS_EQ\n");
    return -1;
  }
}

struct ps_value_t *PS_EQ(const struct ps_value_t *v) {
  int ret;

  if ((ret = PS_EqRaw(v)) < 0)
    return NULL;
  
  return PS_NewBoolean(ret);
}

struct ps_value_t *PS_NEQ(const struct ps_value_t *v) {
  int ret;

  if ((ret = PS_EqRaw(v)) < 0)
    return NULL;
  
  return PS_NewBoolean(!ret);
}

struct ps_value_t *PS_Not(const struct ps_value_t *v) {
  enum ps_type_t type;
  
  if (VerifyArgs(v, 1, 1, &type) < 0)
    return NULL;
  
  switch (type) {
  case t_boolean:
    return PS_NewBoolean(!PS_AsBoolean(PS_GetItem(v, 0)));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Not\n");
      return NULL;
  }
}

struct ps_value_t *PS_Or(const struct ps_value_t *v) {
  enum ps_type_t type;
  
  if (VerifyArgs(v, 2, 2, &type) < 0)
    return NULL;
  
  switch (type) {
  case t_boolean:
    return PS_NewBoolean(INFIX(||, PS_AsBoolean));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Or\n");
      return NULL;
  }
}

struct ps_value_t *PS_And(const struct ps_value_t *v) {
  enum ps_type_t type;
  
  if (VerifyArgs(v, 2, 2, &type) < 0)
    return NULL;
  
  switch (type) {
  case t_boolean:
    return PS_NewBoolean(INFIX(&&, PS_AsBoolean));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_And\n");
      return NULL;
  }
}

#define FLOAT1_FUNC(name, floatr)			\
  struct ps_value_t * name (const struct ps_value_t *v) {	\
    enum ps_type_t type;				\
    							\
    if (VerifyArgs(v, 1, 1, &type) < 0)			\
      return NULL;					\
    							\
    switch (type) {					\
    case t_boolean:					\
    case t_integer:					\
    case t_float:					\
    case t_string:					\
      return floatr;					\
      							\
    default:						\
      fprintf(stderr, "Wrong type args to function " #name "\n");	\
      return NULL;					\
    }							\
  }							\

FLOAT1_FUNC(PS_Int, PS_NewInteger((int) F1));
FLOAT1_FUNC(PS_Ceiling, PS_NewFloat(ceil(F1)));
FLOAT1_FUNC(PS_Floor, PS_NewFloat(floor(F1)));
FLOAT1_FUNC(PS_Log, PS_NewFloat(log(F1)));
FLOAT1_FUNC(PS_Radians, PS_NewFloat(F1*M_PI/180.0));
FLOAT1_FUNC(PS_Sqrt, PS_NewFloat(sqrt(F1)));
FLOAT1_FUNC(PS_Tan, PS_NewFloat(tan(F1)));

struct ps_value_t *PS_Round(const struct ps_value_t *v) {
  enum ps_type_t type;
  ssize_t args;
  double scale;
  
  if ((args = VerifyArgs(v, 1, 2, &type)) < 0)
    return NULL;

  switch (type) {
  case t_boolean:
  case t_integer:
  case t_float:
  case t_string:
    if (args == 1)
      return PS_NewFloat(round(F1));

    scale = pow(10.0, F2);
    return PS_NewFloat(round(F1 * scale) / scale);
    
  default:
    fprintf(stderr, "Wrong type args to function PS_Round\n");
    return NULL;
  }
}

static struct ps_value_t *Reduce(const ps_func_t func, const struct ps_value_t *v) {
  struct ps_value_t *ret, *rr;
  size_t len, count;
  
  if ((len = PS_ItemCount(v)) == 0)
    goto err;
  
  ret = PS_CopyValue(PS_GetItem(v, 0));
  
  for (count = 1; count < len; count++) {
    if (ret == NULL)
      goto err;
    
    rr = PS_Call2(func, ret, PS_GetItem(v, count));
    PS_FreeValue(ret);
    ret = rr;
  }

  return ret;
  
 err:
  return NULL;
}

struct ps_value_t *PS_Max(const struct ps_value_t *v) {
  struct ps_value_t *bb, *ret;
  enum ps_type_t type;
  ssize_t len;
  
  if ((len = VerifyArgs(v, 1, 2, &type)) < 0)
    return NULL;
  
  switch (type) {
  case t_boolean:
  case t_integer:
  case t_float:
  case t_string:
    if (len != 2) {
      fprintf(stderr, "Wrong number args to function PS_Max\n");
      return NULL;
    }
    
    if ((bb = PS_GE(v)) == NULL)
      return NULL;

    ret = PS_CopyValue(PS_GetItem(v, PS_AsBoolean(bb) ? 0 : 1));
    PS_FreeValue(bb);
    
    return ret;
    
  case t_list:
    if (len != 1) {
      fprintf(stderr, "Wrong number args to function PS_Max\n");
      return NULL;
    }
    
    return Reduce(PS_Max, PS_GetItem(v, 0));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Max\n");
      return NULL;
  }
}

struct ps_value_t *PS_Min(const struct ps_value_t *v) {
  struct ps_value_t *bb, *ret;
  enum ps_type_t type;
  ssize_t len;
  
  if ((len = VerifyArgs(v, 1, 2, &type)) < 0)
    return NULL;
  
  switch (type) {
  case t_boolean:
  case t_integer:
  case t_float:
  case t_string:
    if (len != 2) {
      fprintf(stderr, "Wrong number args to function PS_Min\n");
      return NULL;
    }
    
    if ((bb = PS_LE(v)) == NULL)
      return NULL;

    ret = PS_CopyValue(PS_GetItem(v, PS_AsBoolean(bb) ? 0 : 1));
    PS_FreeValue(bb);
    
    return ret;
    
  case t_list:
    if (len != 1) {
      fprintf(stderr, "Wrong number args to function PS_Min\n");
      return NULL;
    }
    
    return Reduce(PS_Min, PS_GetItem(v, 0));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Min\n");
      return NULL;
  }
}

struct ps_value_t *PS_Sum(const struct ps_value_t *v) {
  enum ps_type_t type;
  
  if (VerifyArgs(v, 1, 1, &type) < 0)
    return NULL;
  
  switch (type) {
  case t_list:
    return Reduce(PS_Add, PS_GetItem(v, 0));
    
  default:
      fprintf(stderr, "Wrong type args to function PS_Sum\n");
      return NULL;
  }
}

struct ps_value_t *PS_ThenIfElse(const struct ps_value_t *v, struct ps_context_t *ctx) {
  struct ps_value_t *cond, *ret;
  
  if (v == NULL || PS_GetType(v) != t_function || PS_ItemCount(v) != 4)
    return NULL;

  if ((cond = PS_Eval(PS_GetItem(v, 2), ctx)) == NULL)
    return NULL;
  
  ret = PS_Eval(PS_GetItem(v, PS_AsBoolean(cond) ? 1 : 3), ctx);
  PS_FreeValue(cond);
  return ret;  
}

struct ps_value_t *PS_ResolveOrValue(const struct ps_value_t *v, struct ps_context_t *ctx) {
  if (v == NULL || PS_GetType(v) != t_function || PS_ItemCount(v) != 2)
    return NULL;

  return PS_Eval(PS_GetItem(v, 1), ctx);
}

struct ps_value_t *PS_ExtruderValue(const struct ps_value_t *v, struct ps_context_t *ctx) {
  struct ps_value_t *ext, *ret;
  
  if (v == NULL || PS_GetType(v) != t_function || PS_ItemCount(v) != 3)
    goto err;

  if ((ext = PS_Eval(PS_GetItem(v, 1), ctx)) == NULL)
    goto err;

  if (PS_GetType(ext) != t_string) {
    fprintf(stderr, "Extruder name must be a string\n");
    goto err2;
  }
  
  if (PS_CtxPush(ctx, PS_GetString(ext)) < 0)
    goto err2;
  
  ret = PS_Eval(PS_GetItem(v, 2), ctx);
  
  PS_CtxPop(ctx);
  PS_FreeValue(ext);
  
  return ret;
  
 err2:
  PS_FreeValue(ext);
 err:
  return NULL;
}

struct ps_value_t *PS_ExtruderValues(const struct ps_value_t *v, struct ps_context_t *ctx) {
  const struct ps_value_t *arg;
  
  if (v == NULL || PS_GetType(v) != t_function || PS_ItemCount(v) != 2)
    return NULL;

  if ((arg = PS_GetItem(v, 1)) == NULL)
    return NULL;
  
  if (PS_GetType(arg) != t_variable)
    return NULL;

  return PS_CtxLookupAll(ctx, PS_GetString(arg));
}
