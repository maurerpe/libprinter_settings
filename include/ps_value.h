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

#ifndef PS_VALUE_H
#define PS_VALUE_H

#include "ps_ostream.h"

enum ps_type_t {
  t_null,
  t_boolean,
  t_integer,
  t_float,
  t_string,
  t_variable,
  t_list,
  t_function,
  t_object
};

struct ps_value_t;

struct ps_value_t *PS_NewNull(void);
struct ps_value_t *PS_NewBoolean(int v);
struct ps_value_t *PS_NewInteger(int64_t v);
struct ps_value_t *PS_NewFloat(double v);
struct ps_value_t *PS_NewString(const char *v);
struct ps_value_t *PS_NewStringLen(const char *v, size_t len);
struct ps_value_t *PS_NewVariable(const char *v);
struct ps_value_t *PS_NewVariableLen(const char *v, size_t len);
struct ps_value_t *PS_NewList(void);
struct ps_value_t *PS_NewFunction(const char *name);
struct ps_value_t *PS_NewObject(void);
void PS_FreeValue(struct ps_value_t *v);

enum ps_type_t PS_GetType(const struct ps_value_t *v);
int PS_IsNumeric(const struct ps_value_t *v);
int PS_AsBoolean(const struct ps_value_t *v);
int64_t PS_AsInteger(const struct ps_value_t *v);
double PS_AsFloat(const struct ps_value_t *v);
const char *PS_GetString(const struct ps_value_t *str_var);
int PS_IsScalar(const struct ps_value_t *v);
size_t PS_ItemCount(const struct ps_value_t *list_func_obj);
struct ps_value_t *PS_GetItem(const struct ps_value_t *list, ssize_t pos);
struct ps_value_t *PS_GetMember(const struct ps_value_t *obj, const char *name, int *is_present);

struct ps_value_t *PS_AddRef(const struct ps_value_t *v);
struct ps_value_t *PS_CopyValue(const struct ps_value_t *v);
void PS_StringToVariable(struct ps_value_t *v);
void PS_VariableToString(struct ps_value_t *v);
int PS_AppendToString(struct ps_value_t *str, const char *append); /* Slow */
int PS_AppendToList(struct ps_value_t *list, struct ps_value_t *v);
struct ps_value_t *PS_PopFromList(struct ps_value_t *list);
int PS_PrependToList(struct ps_value_t *list, struct ps_value_t *v); /* Slow */
int PS_SetItem(struct ps_value_t *list, size_t pos, struct ps_value_t *v);
int PS_ResizeList(struct ps_value_t *list, size_t new_size, const struct ps_value_t *fill);
int PS_AddMember(struct ps_value_t *obj, const char *name, struct ps_value_t *v);
int PS_RemoveMember(struct ps_value_t *obj, const char *name);

ssize_t PS_WriteValue(struct ps_ostream_t *os, const struct ps_value_t *v);
ssize_t PS_WriteValuePretty(struct ps_ostream_t *os, const struct ps_value_t *v);

void PS_ValueForeach(const struct ps_value_t *v, void (*func)(const char *, struct ps_value_t **, void *), void *ref_data);

struct ps_value_iterator_t;
struct ps_value_iterator_t *PS_NewValueIterator(const struct ps_value_t *v);
void PS_FreeValueIterator(struct ps_value_iterator_t *vi);
int PS_ValueIteratorNext(struct ps_value_iterator_t *vi);
const char *PS_ValueIteratorKey(const struct ps_value_iterator_t *vi);
struct ps_value_t *PS_ValueIteratorData(const struct ps_value_iterator_t *vi);

#endif
