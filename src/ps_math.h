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

#ifndef PS_MATH_H
#define PS_MATH_H

#include "ps_value.h"
#include "ps_context.h"

typedef struct ps_value_t *(*ps_func_t)(const struct ps_value_t *);
struct ps_value_t *PS_Call1(const ps_func_t func, const struct ps_value_t *v1);
struct ps_value_t *PS_Call2(const ps_func_t func, const struct ps_value_t *v1, const struct ps_value_t *v2);

struct ps_value_t *PS_Expt(const struct ps_value_t *v);
struct ps_value_t *PS_Mul(const struct ps_value_t *v);
struct ps_value_t *PS_Div(const struct ps_value_t *v);
struct ps_value_t *PS_Add(const struct ps_value_t *v);
struct ps_value_t *PS_Sub(const struct ps_value_t *v);
struct ps_value_t *PS_LT(const struct ps_value_t *v);
struct ps_value_t *PS_GT(const struct ps_value_t *v);
struct ps_value_t *PS_LE(const struct ps_value_t *v);
struct ps_value_t *PS_GE(const struct ps_value_t *v);
struct ps_value_t *PS_EQ(const struct ps_value_t *v);
struct ps_value_t *PS_NEQ(const struct ps_value_t *v);
struct ps_value_t *PS_Not(const struct ps_value_t *v);
struct ps_value_t *PS_Or(const struct ps_value_t *v);
struct ps_value_t *PS_And(const struct ps_value_t *v);

struct ps_value_t *PS_Int(const struct ps_value_t *v);
struct ps_value_t *PS_Ceiling(const struct ps_value_t *v);
struct ps_value_t *PS_Floor(const struct ps_value_t *v);
struct ps_value_t *PS_Log(const struct ps_value_t *v);
struct ps_value_t *PS_Radians(const struct ps_value_t *v);
struct ps_value_t *PS_Sqrt(const struct ps_value_t *v);
struct ps_value_t *PS_Tan(const struct ps_value_t *v);
struct ps_value_t *PS_Max(const struct ps_value_t *v);
struct ps_value_t *PS_Min(const struct ps_value_t *v);
struct ps_value_t *PS_Round(const struct ps_value_t *v);
struct ps_value_t *PS_Sum(const struct ps_value_t *v);

struct ps_value_t *PS_DEP(const struct ps_value_t *v);

struct ps_value_t *PS_ThenIfElse(const struct ps_value_t *v, struct ps_context_t *ctx);
struct ps_value_t *PS_ResolveOrValue(const struct ps_value_t *v, struct ps_context_t *ctx);
struct ps_value_t *PS_ExtruderValue(const struct ps_value_t *v, struct ps_context_t *ctx);
struct ps_value_t *PS_ExtruderValues(const struct ps_value_t *v, struct ps_context_t *ctx);

#endif
