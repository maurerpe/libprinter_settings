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

#ifndef PRINTER_SETTINGS_H
#define PRINTER_SETTINGS_H

#include <stdlib.h>
#include <stdio.h>

#if defined (__cplusplus)
extern "C" {
#endif

#include "ps_value.h"
#include "ps_slice.h"
#include "ps_ostream.h"
#include "ps_parse_json.h"

struct ps_value_t *PS_New(const char *printer, const struct ps_value_t *search);

const char *PS_GetPrinter(const struct ps_value_t *ps);
const struct ps_value_t *PS_GetSearch(const struct ps_value_t *ps);

struct ps_value_t *PS_ListExtruders(const struct ps_value_t *ps);
struct ps_value_t *PS_GetDefaults(const struct ps_value_t *ps);
const struct ps_value_t *PS_GetSettingProperties(const struct ps_value_t *ps, const char *extruder, const char *setting);

struct ps_value_t *PS_BlankSettings(const struct ps_value_t *ps);
int PS_AddSetting(struct ps_value_t *set, const char *ext, const char *name, const struct ps_value_t *value);
int PS_MergeSettings(struct ps_value_t *dest, struct ps_value_t *src);
int PS_PruneSettings(struct ps_value_t *settings, const struct ps_value_t *dflt);
  
struct ps_value_t *PS_EvalAll(const struct ps_value_t *ps, const struct ps_value_t *settings);
struct ps_value_t *PS_EvalAllDflt(const struct ps_value_t *ps, const struct ps_value_t *settings, const struct ps_value_t *dflt);

#if defined (__cplusplus)
}
#endif

#endif
