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

#ifndef PS_SLICE_H
#define PS_SLICE_H

#include "ps_value.h"
#include "ps_ostream.h"

struct ps_slice_file_t {
  char *model_file;
  struct ps_value_t *model_settings;
};

struct ps_slice_str_t {
  char *model_str;
  size_t model_str_len;
  struct ps_value_t *model_settings;
};

int PS_SliceFiles(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const struct ps_slice_file_t *files, size_t num_files);
int PS_SliceStrs(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const struct ps_slice_str_t *strs, size_t num_str);

int PS_SliceFile(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_file);
int PS_SliceStr(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_str, size_t model_str_len);

#endif
