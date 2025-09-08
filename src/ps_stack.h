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

#ifndef PS_STACK_H
#define PS_STACK_H

#include "ps_value.h"

enum ps_grouping_t {
  pg_base,
  pg_paren,
  pg_square
};

struct ps_value_t *PS_NewStack(void);

size_t PS_StackLength(struct ps_value_t *stack);
size_t PS_StackArgLength(struct ps_value_t *stack);
int PS_ExpandStack(struct ps_value_t *stack, size_t new_level);
int PS_PushStack(struct ps_value_t *stack, struct ps_value_t *v);
struct ps_value_t *PS_CollapseStack(struct ps_value_t *stack, ssize_t new_level);
int PS_OpenGrouping(struct ps_value_t *stack, enum ps_grouping_t grp, const struct ps_value_t *func);
struct ps_value_t *PS_CloseGrouping(struct ps_value_t *stack, enum ps_grouping_t grp, int *was_func);

#endif
