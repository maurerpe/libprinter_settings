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

#include "ps_eval.h"
#include "ps_ostream.h"

struct ps_value_t *ParseTest(const char *str, const char *ext) {
  struct ps_ostream_t *os;
  struct ps_value_t *val, *expr, *dep, *v;

  if ((os = PS_NewStrOStream()) == NULL)
    exit(1);

  if ((val = PS_NewString(str)) == NULL)
    exit(1);
  
  if ((dep = PS_NewObject()) == NULL)
    exit(1);

  if ((v = PS_NewObject()) == NULL)
    exit(1);

  if (PS_AddMember(dep, "#global", v) < 0)
    exit(1);

  if ((v = PS_NewObject()) == NULL)
    exit(1);

  if (PS_AddMember(dep, "0", v) < 0)
    exit(1);
  
  if ((v = PS_NewObject()) == NULL)
    exit(1);

  if (PS_AddMember(dep, "1", v) < 0)
    exit(1);
  
  if ((expr = PS_ParseForEval(val, ext, dep)) == NULL) {
    fprintf(stderr, "Error parsing string '%s'\n", str);
    exit(1);
  }

  if (PS_WriteValue(os, expr) < 0)
    exit(1);
  
  printf("'%s' -> '%s'\n", str, PS_OStreamContents(os));

  PS_OStreamReset(os);
  if (PS_WriteValue(os, dep) < 0)
    exit(1);
  
  printf("    Dep: '%s'\n", PS_OStreamContents(os));
  
  PS_FreeValue(dep);
  return expr;
}

void EvalTest2(struct ps_value_t *expr, const char *ext, struct ps_value_t *test, const char *ext2, struct ps_value_t *test2) {
  struct ps_value_t *dflt, *v, *result;
  struct ps_context_t *ctx;
  struct ps_ostream_t *os;

  if ((os = PS_NewStrOStream()) == NULL)
    exit(1);

  if ((dflt = PS_NewObject()) == NULL)
    exit(1);
  
  if ((v = PS_NewObject()) == NULL)
    exit(1);

  if (PS_AddMember(dflt, "#global", v) < 0)
    exit(1);
  
  if (PS_AddBuiltin(v, NULL) < 0)
    exit(1);
  
  if ((v = PS_NewObject()) == NULL)
    exit(1);

  if (PS_AddMember(dflt, "0", v) < 0)
    exit(1);
  
  if ((v = PS_NewObject()) == NULL)
    exit(1);

  if (PS_AddMember(dflt, "1", v) < 0)
    exit(1);

  if (PS_AddMember(PS_GetMember(dflt, ext, NULL), "test", PS_AddRef(test)) < 0)
    exit(1);
  
  if (ext2 && test2)
    if (PS_AddMember(PS_GetMember(dflt, ext2, NULL), "test", PS_AddRef(test2)) < 0)
      exit(1);
  
  if ((ctx = PS_NewCtx(NULL, dflt)) == NULL)
    exit(1);
  PS_FreeValue(dflt);
  
  if (PS_CtxPush(ctx, ext) < 0)
    exit(1);
  
  if ((result = PS_Eval(expr, ctx)) == NULL) {
    PS_WriteValue(os, test);
    fprintf(stderr, "Could not evaluate with value '%s'\n", PS_OStreamContents(os));
    PS_FreeOStream(os);
    PS_FreeCtx(ctx);
    PS_FreeValue(test);
    return;
  }

  PS_WriteValue(os, test);
  printf("Eval with '%s' -> ", PS_OStreamContents(os));
  PS_OStreamReset(os);
  PS_WriteValue(os, result);
  printf("'%s'\n", PS_OStreamContents(os));
  
  PS_FreeOStream(os);
  PS_FreeCtx(ctx);
  PS_FreeValue(test);
  PS_FreeValue(result);
}

void EvalTest(struct ps_value_t *expr, const char *ext, struct ps_value_t *test) {
  EvalTest2(expr, ext, test, NULL, NULL);
}

int main(void) {
  struct ps_value_t *v;
  
  v = ParseTest("defaultExtruderPosition()", "#global");
  EvalTest(v, "#global", PS_NewFloat(0.313));
  
  v = ParseTest("5 + test", "#global");
  EvalTest(v, "#global", PS_NewFloat(0.313));
  EvalTest(v, "#global", PS_NewInteger(3921));
  EvalTest(v, "#global", PS_NewString("5.21"));
  PS_FreeValue(v);
  
  v = ParseTest("5 + 3*4**test+2*3", "#global");
  EvalTest(v, "#global", PS_NewInteger(10));
  EvalTest(v, "#global", PS_NewFloat(-1));
  PS_FreeValue(v);
  
  v = ParseTest("5 + 3*4**(test*4+2*3)+2*3", "#global");
  EvalTest(v, "#global", PS_NewInteger(2));
  EvalTest(v, "#global", PS_NewFloat(1.2));
  PS_FreeValue(v);
  
  v = ParseTest("test + math.pi", "#global");
  EvalTest(v, "#global", PS_NewInteger(3));
  PS_FreeValue(v);
  
  v = ParseTest("round(3/test,4)+2", "1");
  EvalTest(v, "1", PS_NewFloat(1.7));
  PS_FreeValue(v);
  
  v = ParseTest("3 if test + 3 > 4 else 'hi'", "#global");
  EvalTest(v, "#global", PS_NewInteger(0));
  EvalTest(v, "#global", PS_NewInteger(100));
  PS_FreeValue(v);
  
  v = ParseTest("resolveOrValue('test')+2", "#global");
  EvalTest(v, "#global", PS_NewFloat(1.7));
  PS_FreeValue(v);
  
  v = ParseTest("extruderValue('1','test')+2", "#global");
  EvalTest(v, "1", PS_NewFloat(1.7));
  PS_FreeValue(v);
  
  v = ParseTest("len(extruderValues('test'))", "#global");
  EvalTest2(v, "0", PS_NewFloat(3.1), "1", PS_NewFloat(1.7));
  PS_FreeValue(v);
  
  v = ParseTest("extruderValues('test')", "#global");
  EvalTest2(v, "0", PS_NewFloat(1.7), "1", PS_NewString("hi"));
  PS_FreeValue(v);
  
  v = ParseTest("map(abs, extruderValues('test'))", "#global");
  EvalTest2(v, "0", PS_NewFloat(-3.14), "1", PS_NewFloat(3));
  PS_FreeValue(v);
  
  v = ParseTest("test in ['hi', 3]", "#global");
  EvalTest(v, "#global", PS_NewInteger(3));
  EvalTest(v, "#global", PS_NewString("hi"));
  EvalTest(v, "#global", PS_NewString("not in list"));
  PS_FreeValue(v);
  
  return 0;
}
