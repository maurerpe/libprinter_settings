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
#include "ps_context.h"
#include "ps_slice.h"
#include "ps_exec.h"
#include "ps_math.h"

struct args_t {
  char **a;
  size_t num_args;
  size_t num_alloc;
};

#define INIT_ARGS 64

static int InitArgs(struct args_t *args) {
  memset(args, 0, sizeof(*args));
  
  if ((args->a = calloc(INIT_ARGS, sizeof(*args->a))) == NULL)
    return -1;
  
  args->num_alloc = INIT_ARGS;
  
  return 0;
}

static int AddArg(struct args_t *args, const char *str) {
  char **b, *ds;
  size_t alloc;
  
  if (args->num_args + 1 >= args->num_alloc) {
    alloc = args->num_alloc << 1;
    if (alloc < args->num_alloc)
      return -1;
    
    if ((b = calloc(alloc, sizeof(*b))) == NULL)
      return -1;

    memcpy(b, args->a, args->num_args * sizeof(*b));
    free(args->a);
    args->a = b;
    args->num_alloc = alloc;
  }
  
  if ((ds = strdup(str)) == NULL)
    return -1;
  
  args->a[args->num_args++] = ds;
  
  return 0;
}

#ifdef DEBUG
static void PrintArgs(struct args_t *args) {
  char **a;

  printf("Using args:\n");
  for (a = args->a; *a != NULL; a++) {
    printf("   %s\n", *a);
  }
}
#endif

static void DestroyArgs(struct args_t *args) {
  char **a = args->a;
  size_t count = args->num_args;
  
  while (count--) {
    free(*a);
    a++;
  }
  
  free(args->a);
}

static int AddSettings(struct args_t *args, const struct ps_value_t *settings) {
  struct ps_ostream_t *os;
  struct ps_value_iterator_t *vi_ext, *vi_set;
  struct ps_value_t *val;
  const char *ext, *name;
  
  if ((os = PS_NewStrOStream()) == NULL)
    goto err;
  
  if ((vi_ext = PS_NewValueIterator(settings)) == NULL)
    goto err2;
  
  while (PS_ValueIteratorNext(vi_ext)) {
    ext = PS_ValueIteratorKey(vi_ext);
    
    if (strcmp(ext, "#global") != 0) {
      PS_OStreamReset(os);
      if (PS_WriteStr(os, "-e") < 0)
	goto err3;
      if (PS_WriteStr(os, ext) < 0)
	goto err3;
      if (AddArg(args, PS_OStreamContents(os)) < 0)
	goto err3;
    }
	       
    if ((vi_set = PS_NewValueIterator(PS_ValueIteratorData(vi_ext))) == NULL)
      goto err3;
    
    while (PS_ValueIteratorNext(vi_set)) {
      name = PS_ValueIteratorKey(vi_set);
      val = PS_ValueIteratorData(vi_set);
      
      if (AddArg(args, "-s") < 0)
	goto err4;
      PS_OStreamReset(os);
      if (PS_WriteStr(os, name) < 0)
	goto err4;
      if (PS_WriteChar(os, '=') < 0)
	goto err4;
      if (PS_GetType(val) == t_string) {
	if (PS_WriteStr(os, PS_GetString(val)) < 0)
	  goto err4;
      } else {
	if (PS_WriteValue(os, val) < 0)
	  goto err4;
      }
      if (AddArg(args, PS_OStreamContents(os)) < 0)
	goto err4;
    }
    
    PS_FreeValueIterator(vi_set);
  }
  
  PS_FreeValueIterator(vi_ext);
  PS_FreeOStream(os);
  return 0;
  
 err4:
  PS_FreeValueIterator(vi_set);
 err3:
  PS_FreeValueIterator(vi_ext);
 err2:
  PS_FreeOStream(os);
 err:
  return -1;
}

static int BuildArgs(struct args_t *args, const struct ps_value_t *ps, const struct ps_value_t *settings, const struct ps_slice_file_t *files, size_t num_files, const char *out_file) {
  struct ps_value_t *set, *dflt, *model_set;
  size_t count;
  
  if (AddArg(args, "CuraEngine") < 0)
    goto err;

  if (AddArg(args, "slice") < 0)
    goto err;

#ifdef DEBUG
  if (AddArg(args, "-v") < 0)
    goto err;
  
  if (AddArg(args, "-m1") < 0)
    goto err3;
#endif

  if (AddArg(args, "-j") < 0)
    goto err;
  
  if (AddArg(args, PS_GetPrinter(ps)) < 0)
    goto err;
  
  if (AddArg(args, "-o") < 0)
    goto err;

  if (AddArg(args, out_file) < 0)
    goto err;

  if ((dflt = PS_GetDefaults(ps)) == NULL)
    goto err;
  
  if ((set = PS_EvalAllDflt(ps, settings, dflt)) == NULL)
    goto err2;

  printf("Pruning global settings\n");
  if (PS_MergeSettings(dflt, set) < 0)
    goto err3;

  if (AddSettings(args, dflt) < 0)
    goto err3;
  
  for (count = 0; count < num_files; count++) {
    if (AddArg(args, "-l") < 0)
      goto err3;
    if (AddArg(args, files[count].model_file) < 0)
      goto err3;

    if (files[count].model_settings) {
      if ((model_set = PS_EvalAllDflt(ps, files[count].model_settings, dflt)) == NULL)
	goto err3;

      printf("Pruning settings for model %zd\n", count);
      if ((PS_PruneSettings(model_set, dflt)) < 0)
	goto err4;
      
      if (AddSettings(args, model_set) < 0)
	goto err4;
      PS_FreeValue(model_set);
    }
  }
  
  PS_FreeValue(set);
  PS_FreeValue(dflt);
  return 0;

 err4:
  PS_FreeValue(model_set);
 err3:
  PS_FreeValue(set);
 err2:
  PS_FreeValue(dflt);
 err:
  return -1;
}

int PS_SliceFiles(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const struct ps_slice_file_t *files, size_t num_files) {
  struct args_t args;
  struct ps_ostream_t *os;
  struct ps_out_file_t *of;
  
  if ((os = PS_NewFileOStream(stdout)) != NULL) {
    printf("Using base settings:\n");
    PS_WriteValue(os, settings);
    printf("\n");
    PS_FreeOStream(os);
  }
  
  if ((of = PS_OutFile_New()) == NULL)
    goto err;
  
  if (InitArgs(&args) < 0)
    goto err2;
  
  if (BuildArgs(&args, ps, settings, files, num_files, PS_OutFile_GetName(of)) < 0)
    goto err3;

#ifdef DEBUG
  PrintArgs(&args);
#endif
  
  if (PS_ExecArgs(args.a, NULL, NULL, PS_GetSearch(ps)) < 0)
    goto err3;

  if (PS_OutFile_ReadToStream(of, gcode) < 0)
    goto err3;

  DestroyArgs(&args);
  PS_OutFile_Free(of);
  return 0;

 err3:
  DestroyArgs(&args);
 err2:
  PS_OutFile_Free(of);
 err:
  return -1;
}

int PS_SliceStrs(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const struct ps_slice_str_t *strs, size_t num_str) {
  struct ps_slice_file_t *files;
  size_t count;
  int ret;
  
  if ((files = calloc(num_str, sizeof(*files))) == NULL)
    goto err;
  
  for (count = 0; count < num_str; count++) {
    if ((files[count].model_file = PS_WriteToTempFile(strs[count].model_str, strs[count].model_str_len)) == NULL)
      goto err2;
    files[count].model_settings = strs[count].model_settings;
  }

  ret = PS_SliceFiles(gcode, ps, settings, files, num_str);
  
  for (count = 0; count < num_str; count++) {
    PS_DeleteFile(files[count].model_file);
    free(files[count].model_file);
  }
  
  free(files);
  return ret;
  
 err2:
  while (count-- > 0) {
    PS_DeleteFile(files[count].model_file);
    free(files[count].model_file);
  }
  free(files);
 err:
  return -1;
}

int PS_SliceFile(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_file) {
  struct ps_slice_file_t file;

  file.model_file = (char *) model_file;
  file.model_settings = NULL;
  
  return PS_SliceFiles(gcode, ps, settings, &file, 1);
}

int PS_SliceStr(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_str, size_t model_str_len) {
  struct ps_slice_str_t str;

  str.model_str = (char *) model_str;
  str.model_str_len = model_str_len;
  str.model_settings = NULL;
  
  return PS_SliceStrs(gcode, ps, settings, &str, 1);
}
