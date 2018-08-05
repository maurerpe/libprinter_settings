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

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "ps_math.h"
#include "ps_eval.h"
#include "ps_ostream.h"
#include "ps_stack.h"

#define UNA 8
#define EXP 7
#define MUL 6
#define ADD 5
#define CMP 4
#define ULG 3
#define LOG 2
#define IFE 1
#define FUN 0

enum expr_type_t {
  e_init,
  e_operator,
  e_boolean,
  e_number,
  e_string,
  e_bareword,
  e_expr,
  e_null,
  e_error,
  e_end
};

static const char *expr_name[] = {
  "init",
  "operator",
  "boolean",
  "number",
  "string",
  "bareword",
  "expression",
  "null",
  "error",
  "end"
};

struct macro_prop_t {
  char *name;
  struct ps_value_t *(*func)(const struct ps_value_t *, struct ps_context_t *);
  int (*verify)(struct ps_value_t **v, const char *, struct ps_value_t *);
  size_t min_arg;
  size_t max_arg;
};

static int VerifyExtruderValue(struct ps_value_t **v, const char *ext, struct ps_value_t *dep);
static int VerifyExtruderValues(struct ps_value_t **v, const char *ext, struct ps_value_t *dep);
static int VerifyResolveOrValue(struct ps_value_t **v, const char *ext, struct ps_value_t *dep);

const struct macro_prop_t macro_prop[] =
  {{"if",             PS_ThenIfElse,     NULL,                 3, 3},
   {"extruderValue",  PS_ExtruderValue,  VerifyExtruderValue,  2, 2},
   {"extruderValues", PS_ExtruderValues, VerifyExtruderValues, 1, 1},
   {"resolveOrValue", PS_ResolveOrValue, VerifyResolveOrValue, 1, 1}};

struct oper_prop_t {
  char *name;
  struct ps_value_t *(*func)(const struct ps_value_t *);
  int level;
  int num_args;
};

const struct oper_prop_t oper_prop[] =
  {{"**",   PS_Expt, EXP,  2},
   {"*",    PS_Mul,  MUL,  2},
   {"/",    PS_Div,  MUL,  2},
   {"+",    PS_Add,  ADD,  2},
   {"-",    PS_Sub,  ADD,  2},
   {"<",    PS_LT,   CMP,  2},
   {">",    PS_GT,   CMP,  2},
   {"<=",   PS_LE,   CMP,  2},
   {">=",   PS_GE,   CMP,  2},
   {"==",   PS_EQ,   CMP,  2},
   {"!=",   PS_NEQ,  CMP,  2},
   {"not",  PS_Not,  ULG,  1},
   {"or",   PS_Or,   LOG,  2},
   {"and",  PS_And,  LOG,  2},
   {"if",   NULL,    IFE,  3},
   {"else", NULL,    IFE, -1},
   {",",    NULL,    FUN, -1}};

struct func_prop_t {
  char *name;
  struct ps_value_t *(*func)(const struct ps_value_t *);
  size_t min_arg;
  size_t max_arg;
};

const struct func_prop_t func_prop[] =
  {{"defaultExtruderPosition", PS_DEP,     0, 0},
   {"int",                     PS_Int,     1, 1},
   {"math.ceil",               PS_Ceiling, 1, 1},
   {"math.floor",              PS_Floor,   1, 1},
   {"math.log",                PS_Log,     1, 1},
   {"math.radians",            PS_Radians, 1, 1},
   {"math.sqrt",               PS_Sqrt,    1, 1},
   {"math.tan",                PS_Tan,     1, 1},
   {"max",                     PS_Max,     1, 2},
   {"min",                     PS_Min,     1, 2},
   {"round",                   PS_Round,   1, 2},
   {"sum",                     PS_Sum,     1, 1}};

static struct ps_value_t *FuncEval(const struct ps_value_t *v, struct ps_context_t *ctx) {
  const char *name;
  struct ps_value_t *ve, *arg, *ret;
  size_t count;
  
  if ((name = PS_GetString(PS_GetItem(v, 0))) == NULL)
    goto err;
  
  for (count = 0; count < sizeof(macro_prop)/sizeof(macro_prop[0]); count++)
    if (strcmp(name, macro_prop[count].name) == 0)
      return macro_prop[count].func(v, ctx);
  
  if ((ve = PS_NewList()) == NULL)
    goto err;
  
  for (count = 1; count < PS_ItemCount(v); count++) {
    if ((arg = PS_Eval(PS_GetItem(v, count), ctx)) == NULL)
      goto err2;

    if (PS_AppendToList(ve, arg) < 0)
      goto err3;
  }
  
  for (count = 0; count < sizeof(oper_prop)/sizeof(oper_prop[0]); count++) {
    if (strcmp(name, oper_prop[count].name) == 0) {
      ret = oper_prop[count].func(ve);
      PS_FreeValue(ve);
      return ret;
    }
  }
  
  for (count = 0; count < sizeof(func_prop)/sizeof(func_prop[0]); count++) {
    if (strcmp(name, func_prop[count].name) == 0) {
      ret = func_prop[count].func(ve);
      PS_FreeValue(ve);
      return ret;
    }
  }

  fprintf(stderr, "Unknown function %s", name);
  goto err2;
  
 err3:
  PS_FreeValue(arg);
 err2:
  PS_FreeValue(ve);
 err:
  return NULL;
}

struct ps_value_t *PS_Eval(const struct ps_value_t *v, struct ps_context_t *ctx) {
  if (v == NULL)
    return NULL;

  switch (PS_GetType(v)) {
  case t_variable:
    return PS_CopyValue(PS_CtxLookup(ctx, PS_GetString(v)));

  case t_function:
    return FuncEval(v, ctx);
    
  default:
    return PS_CopyValue(v);
  }
}

/**************************** Parser *************************************/
static int EvalLastArg(struct ps_value_t **v, const char *ext, struct ps_value_t *dep) {
  struct ps_value_t *arg, *parsed;

  if ((arg = PS_PopFromList(*v)) == NULL)
    goto err;
  
  if ((parsed = PS_ParseForEval(arg, ext, dep)) == NULL)
    goto err2;
  
  if (PS_AppendToList(*v, parsed) < 0)
    goto err3;
  
  PS_FreeValue(arg);
  return 0;

 err3:
  PS_FreeValue(parsed);
 err2:
  PS_FreeValue(arg);
 err:
  return -1;
}

static int VerifyExtruderValue(struct ps_value_t **v, const char *ext, struct ps_value_t *dep) {
  return EvalLastArg(v, NULL, dep);
}

static int VerifyExtruderValues(struct ps_value_t **v, const char *ext, struct ps_value_t *dep) {
  return EvalLastArg(v, NULL, dep);
}

static int VerifyResolveOrValue(struct ps_value_t **v, const char *ext, struct ps_value_t *dep) {
  return EvalLastArg(v, ext, dep);
}

static int VerifyFunc(struct ps_value_t **v, const char *ext, struct ps_value_t *dep) {
  size_t count, num_arg;
  const char *fname;
  
  if (PS_GetType(*v) != t_function)
    return -1;
  
  if ((fname = PS_GetString(PS_GetItem(*v, 0))) == NULL)
    return -1;
  num_arg = PS_ItemCount(*v) - 1;
  
  for (count = 0; count < sizeof(macro_prop)/sizeof(*macro_prop); count++) {
    if (strcmp(fname, macro_prop[count].name) == 0) {
      if (num_arg < macro_prop[count].min_arg || num_arg > macro_prop[count].max_arg) {
	fprintf(stderr, "Incorrect number of args to macro '%s'\n", fname);
	return -1;
      }
      
      if (macro_prop[count].verify)
	return macro_prop[count].verify(v, ext, dep);
      
      return 0;
    }
  }
  
  for (count = 0; count < sizeof(oper_prop)/sizeof(*oper_prop); count++) {
    if (strcmp(fname, oper_prop[count].name) == 0) {
      if (num_arg != oper_prop[count].num_args) {
	fprintf(stderr, "Incorrect number of args to operator '%s'\n", fname);
	return -1;
      }
      
      return 0;
    }
  }
  
  for (count = 0; count < sizeof(func_prop)/sizeof(*func_prop); count++) {
    if (strcmp(fname, func_prop[count].name) == 0) {
      if (num_arg < func_prop[count].min_arg || num_arg > func_prop[count].max_arg) {
	fprintf(stderr, "Incorrect number of args to func '%s'\n", fname);
	return -1;
      }
      
      return 0;
    }
  }
  
  fprintf(stderr, "Unknown macro/operator/function '%s'\n", fname);
  return -1;
}

static int AddDep(struct ps_value_t *v, const char *ext, struct ps_value_t *dep) {
  struct ps_value_t *t, *c;
  struct ps_value_iterator_t *vi;
  
  if ((t = PS_NewBoolean(1)) == NULL)
    goto err;
  
  if (ext) {
    if (PS_AddMember(PS_GetMember(dep, ext, NULL), PS_GetString(v), t) < 0)
      goto err2;
    return 0;
  }
  
  if ((vi = PS_NewValueIterator(dep)) == NULL)
    goto err2;
  
  if (PS_ValueIteratorNext(vi)) {
    while (PS_ValueIteratorNext(vi)) {
      if ((c = PS_CopyValue(t)) == NULL)
	goto err3;
      if (PS_AddMember(PS_ValueIteratorData(vi), PS_GetString(v), c) < 0)
	goto err4;
    }
  }

  PS_FreeValueIterator(vi);
  PS_FreeValue(t);
  return 0;

 err4:
  PS_FreeValue(c);
 err3:
  PS_FreeValueIterator(vi);
 err2:
  PS_FreeValue(t);
 err:
  return -1;
}

static struct ps_value_t *ParseString(const char *str, const char **end) {
  char quote, buf[512], *cur = buf, *buf_end = buf + sizeof(buf) - 3;
  struct ps_ostream_t *os;
  struct ps_value_t *v;
  
  if ((os = PS_NewStrOStream()) == NULL)
    goto err;
  
  quote = *str++;
  
  while (*str != quote) {
    if (*str == '\0') {
      fprintf(stderr, "Unterminated string when parsing expression\n");
      goto err2;
    }
    
    if (cur >= buf_end) {
      if (PS_WriteBuf(os, buf, cur - buf) < 0)
	goto err2;
      cur = buf;
    }
    
    if (*str == '\\') {
      str++;
      switch (*str++) {
      case '\0': goto err2;
      case 'b': *cur++ = '\b'; break;
      case 'f': *cur++ = '\f'; break;
      case 'n': *cur++ = '\n'; break;
      case 'r': *cur++ = '\r'; break;
      case 't': *cur++ = '\t'; break;
      default:  *cur++ = *str; break;
      }
    } else {
      *cur++ = *str++;
    }
  }
  
  *end = str + 1;
  
  if (PS_WriteBuf(os, buf, cur - buf) < 0)
    goto err2;
  
  if ((v = PS_NewString(PS_OStreamContents(os))) == NULL)
    goto err2;
  
  PS_FreeOStream(os);
  
  return v;
  
 err2:
  PS_FreeOStream(os);
 err:
  return NULL;
}

static struct ps_value_t *ParseAtom(enum expr_type_t type, const char *str, const char **end) {
  long long numi;
  double numf;
  char *num_end;
  size_t len;

  len = *end - str;
  switch (type) {
  case e_operator:
    return PS_NewStringLen(str, len);
    
  case e_boolean:
    return PS_NewBoolean(*str == 't');
    
  case e_number:
    errno = 0;
    numi = strtoll(str, &num_end, 0);
    if (errno == 0 && num_end == *end)
      return PS_NewInteger(numi);

    errno = 0;
    numf = strtod(str, &num_end);
    if (errno == 0 && num_end == *end)
      return PS_NewFloat(numf);
    
    return NULL;

  case e_string:
    return ParseString(str, end);
    
  case e_bareword:
    return PS_NewVariableLen(str, len);
    
  case e_null:
    return PS_NewNull();
    
  default:
    return NULL;
  }
}

static enum expr_type_t NextAtom(const char **str, const char **end) {
  const char *cur;
  size_t len;

  if (str == NULL || *str == NULL || end == NULL)
    return e_error;

  cur = *str;

#ifdef DEBUG
  printf("NextAtom Str: %s\n", cur);
#endif
  
  while (*cur != '\0' && isspace(*cur))
    cur++;
  
  *str = cur;
  
  if (*cur == '\0') {
    *end = cur;
    
    return e_end;
  }
  
  switch (*cur) {
  case '*':
    *end = cur[1] == '*' ? cur + 2 : cur + 1;
    return e_operator;
    
  case '<':
  case '>':
    *end = cur[1] == '=' ? cur + 2 : cur + 1;
    return e_operator;

  case '=':
  case '!':
    if (cur[1] != '=') {
      fprintf(stderr, "Invalid operator at '%s'\n", cur);
      return e_error;
    }
    
    *end = cur + 2;
    
    return e_operator;

  case '(':
  case ')':
  case '/':
  case '+':
  case '-':
  case ',':
    *end = cur + 1;

    return e_operator;

  case '"':
  case '\'':
    *end = cur;

    return e_string;
    
  default:
    break;
  }
  
  if (isdigit(*cur)) {
    do {
      cur++;
    } while (isalnum(*cur) || *cur == '_' || *cur == '.');
    *end = cur;
    
    return e_number;
  }
  
  if (isalpha(*cur)) {
    do {
      cur++;
    } while (isalnum(*cur) || *cur == '_' || *cur == '.');
    *end = cur;
    
    len = cur - *str;

    if (len == 2) {
      if (strncmp(*str, "or", len) == 0 ||
	  strncmp(*str, "if", len) == 0)
	return e_operator;
    } else if (len == 3) {
      if (strncmp(*str, "and", len) == 0 ||
	  strncmp(*str, "not", len) == 0)
	return e_operator;
    } if (len == 4) {
      if (strncmp(*str, "else", len) == 0)
	return e_operator;
      if (strncmp(*str, "null", len) == 0)
	return e_null;
      if (strncmp(*str, "true", len) == 0)
	return e_boolean;
    } else if (len == 5) {
      if (strncmp(*str, "false", len) == 0)
	return e_boolean;
    }
    
    return e_bareword;
  }

  fprintf(stderr, "Invalid syntax at '%s'\n", cur);
  return e_error;
}

static int PushStackType(struct ps_value_t *stack, enum expr_type_t type, struct ps_value_t *v, const char *ext, struct ps_value_t *dep) {
  if (type == e_bareword)
    if (AddDep(v, ext, dep) < 0)
      return -1;

  return PS_PushStack(stack, v);
}

static int AddOper(struct ps_value_t *stack, struct ps_value_t *oper, enum expr_type_t prev_type, struct ps_value_t *prev, const char *ext, struct ps_value_t *dep) {
  size_t cur_level, level;
  int num_args, count;
  
  cur_level = PS_StackLength(stack);

#ifdef DEBUG
  printf("AddOper %s\n", PS_GetString(oper));
#endif

  /* Handle open parenthesis */
  if (strcmp(PS_GetString(oper), "(") == 0) {
    PS_FreeValue(oper);
    if (prev_type != e_bareword && prev_type != e_operator && prev_type != e_init) {
      fprintf(stderr, "Open parenthesis cannot follow %s\n", expr_name[prev_type]);
      goto err;
    }
    
    if (prev_type == e_bareword) {
      PS_VariableToString(prev);
      if (PS_OpenGrouping(stack, PS_GetString(prev)) < 0)
	goto err;
      
      PS_FreeValue(prev);
      return 0;
    }
    
    if (PS_OpenGrouping(stack, NULL) < 0)
      goto err;
    
    PS_FreeValue(prev);
    return 0;
  }
  
  /* Determine level and num_args */
  if ((strcmp(PS_GetString(oper), "+") == 0 ||
       strcmp(PS_GetString(oper), "-") == 0) &&
      (prev_type == e_operator || prev_type == e_init)) {
    level = UNA;
    num_args = 1;
  } else {
    count = 0;
    while (1) {
      if (count >= sizeof(oper_prop)/sizeof(*oper_prop)) {
	fprintf(stderr, "Unknown operator '%s'\n", PS_GetString(oper));
	goto err2;
      }
      if (strcmp(PS_GetString(oper), oper_prop[count].name) == 0) {
	level = oper_prop[count].level;
	num_args = oper_prop[count].num_args;
	break;
      }
      count++;
    }
  }
  
  if (num_args == 1) {
    /* Unary */
    if (prev_type != e_init && prev_type != e_operator) {
      fprintf(stderr, "Unary operator '%s' cannot follow %s\n", PS_GetString(oper), expr_name[prev_type]);
      goto err2;
    }
    
    if (cur_level >= level) {
      fprintf(stderr, "Unary operator '%s' must follow operators of lower precidence\n", PS_GetString(oper));
      goto err2;
    }
    
    if ((PS_ExpandStack(stack, level)) < 0)
      goto err2;
    if ((PS_PushStack(stack, oper)) < 0)
      goto err2;
    
    PS_FreeValue(prev);
    return 0;
  }

  /* Binary, trinary */
  if (prev_type == e_init || prev_type == e_operator) {
    fprintf(stderr, "Operator '%s' cannot follow %s\n", PS_GetString(oper), expr_name[prev_type]);
    goto err2;
  }

  if (cur_level >= level) {
    /* Lower precidence operator */
    if (PushStackType(stack, prev_type, prev, ext, dep) < 0)
      goto err2;
    prev = NULL;
    if (num_args >= 0) {
      if ((prev = PS_CollapseStack(stack, level - 1)) == NULL)
	goto err2;
      if (PS_ExpandStack(stack, level) < 0)
	goto err2;
      if (PS_PushStack(stack, oper) < 0)
	goto err2;
      if (PS_PushStack(stack, prev) < 0)
	goto err;

      return 0;
    }
    
    /* Continuation operator e.g. ',' */
    PS_FreeValue(oper);
    if (cur_level > level) {
      if ((prev = PS_CollapseStack(stack, level)) == NULL)
	goto err;
      if (PS_PushStack(stack, prev) < 0)
	goto err;
    }
    return 0;
  }
  
  /* Higher precidence operator */
  if (PS_ExpandStack(stack, level) < 0)
    goto err2;
  if (PS_PushStack(stack, oper) < 0)
    goto err2;
  if (PushStackType(stack, prev_type, prev, ext, dep) < 0)
    goto err;
  
  return 0;

 err2:
  PS_FreeValue(oper);
 err:
  PS_FreeValue(prev);
  return -1;
}

static struct ps_value_t *ParseStr(const char *str, const char *ext, struct ps_value_t *dep) {
  struct ps_value_t *stack, *v, *prev;
  const char *end;
  enum expr_type_t prev_type, type;
  int was_func;
  
  if (str == NULL)
    goto err;
  
  if ((stack = PS_NewStack()) == NULL)
    goto err;

  v = NULL;
  prev = NULL;
  prev_type = e_init;
  while (1) {
    if ((type = NextAtom(&str, &end)) == e_error)
      goto err3;

    if (type == e_end)
      break;

    if ((v = ParseAtom(type, str, &end)) == NULL)
      goto err3;
    
    if (type != e_operator) {
      if (prev_type != e_operator && prev_type != e_init) {
	fprintf(stderr, "Error: %s cannot follow %s\n", expr_name[type], expr_name[prev_type]);
	goto err3;
      }
      prev = v;
      v = NULL;
    } else if (strcmp(PS_GetString(v), ")") == 0) {
      PS_FreeValue(v);
      v = NULL;
      if (prev != NULL) {
	if (PushStackType(stack, prev_type, prev, ext, dep) < 0)
	  goto err3;
	prev = NULL;
      }
      if ((prev = PS_CloseGrouping(stack, 0, &was_func)) == NULL)
	goto err3;
      type = e_expr;
      if (was_func && VerifyFunc(&prev, ext, dep) < 0)
	goto err3;
    } else {
      if (AddOper(stack, v, prev_type, prev, ext, dep) < 0)
	goto err2;
      prev = NULL;
      v = NULL;
    }
    
    prev_type = type;
    str = end;
  }
  
  if (prev_type == e_operator) {
    fprintf(stderr, "Error: expression cannot end with operator\n");
    goto err3;
  }
  
  if (prev && PushStackType(stack, prev_type, prev, ext, dep) < 0)
    goto err3;
  prev = NULL;
  
  if ((v = PS_CloseGrouping(stack, 1, NULL)) == NULL)
    goto err3;
  
  PS_FreeValue(stack);
  return v;

 err3:
  PS_FreeValue(v);
  PS_FreeValue(prev);
 err2:
  PS_FreeValue(stack);
 err:
  return NULL;
}

struct ps_value_t *PS_ParseForEval(const struct ps_value_t *val, const char *ext, struct ps_value_t *dep) {
  switch (PS_GetType(val)) {
  case t_null:
  case t_boolean:
  case t_integer:
  case t_float:
    return PS_CopyValue(val);

  case t_string:
    return ParseStr(PS_GetString(val), ext, dep);

  default:
    fprintf(stderr, "Invalid object to parse\n");
    return NULL;
  }
}
