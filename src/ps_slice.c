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

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#include "printer_settings.h"
#include "printer_settings_internal.h"
#include "ps_context.h"
#include "ps_slice.h"

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
  free(args->a);
}

static int BuildArgs(struct args_t *args, const char *printer, const struct ps_value_t *settings, const char *model_file) {
  struct ps_ostream_t *os;
  struct ps_value_iterator_t *vi_ext, *vi_set;
  struct ps_value_t *val;
  const char *ext, *name;
  
  if (AddArg(args, "CuraEngine") < 0)
    goto err;

  if (AddArg(args, "slice") < 0)
    goto err;

  if (AddArg(args, "-j") < 0)
    goto err;
  
  if (AddArg(args, printer) < 0)
    goto err;
  
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
	  goto err;
      }
      if (AddArg(args, PS_OStreamContents(os)) < 0)
	goto err4;
    }
    
    PS_FreeValueIterator(vi_set);
  }
  
  PS_FreeValueIterator(vi_ext);
  
  if (model_file) {
    if (AddArg(args, "-l") < 0)
      goto err2;
    if (AddArg(args, model_file) < 0)
      goto err2;
  }
  
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

static int WriteStr(int fd, const char *str) {
  size_t len;
  ssize_t count;

  len = strlen(str);
  while (len > 0) {
    if ((count = write(fd, str, len)) < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	continue;
      return -1;
    }

    str += count;
    len -= count;
  }
  
  return 0;
}

static int ReadToStream(int fd, struct ps_ostream_t *os) {
  char buf[4096];
  ssize_t count;
  
  while (1) {
    if ((count = read(fd, buf, sizeof(buf))) < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	continue;
      return -1;
    }
    
    if (count == 0)
      return 0;
    
    if (PS_WriteBuf(os, buf, count) < 0)
      return -1;
  }
}

#define ENV_VAR "CURA_ENGINE_SEARCH_PATH"

static int SetSearchEnv(const struct ps_value_t *search) {
  struct ps_ostream_t *os;
  struct ps_value_iterator_t *vi;
  int is_first;
  
  if ((os = PS_NewStrOStream()) == NULL)
    goto err;
  
  if ((vi = PS_NewValueIterator(search)) == NULL)
    goto err2;

  is_first = 1;
  while (PS_ValueIteratorNext(vi)) {
    if (!is_first && PS_WriteChar(os, ':') < 0)
	goto err3;
    is_first = 0;
    
    if (PS_WriteStr(os, PS_GetString(PS_ValueIteratorData(vi))) < 0)
      goto err3;
  }

#ifdef DEBUG
  fprintf(stderr, "Setting " ENV_VAR "=%s\n", PS_OStreamContents(os));
#endif
  if (setenv(ENV_VAR, PS_OStreamContents(os), 1) < 0) {
    perror("Could not set " ENV_VAR " environment variable");
    goto err3;
  }
  
  PS_FreeValueIterator(vi);
  PS_FreeOStream(os);
  return 0;
  
 err3:
  PS_FreeValueIterator(vi);
 err2:
  PS_FreeOStream(os);
 err:
  return -1;
}

static int ExecArgs(struct args_t *args, const char *stdin_str, struct ps_ostream_t *stdout_os, const struct ps_value_t *search) {
  int in_pipe[2], out_pipe[2], status;
  pid_t pid;

#ifdef DEBUG
  PrintArgs(args);
#endif
  
  if (pipe(in_pipe) < 0)
    goto err;

  if (pipe(out_pipe) < 0)
    goto err2;
  
  switch ((pid = fork())) {
  case -1:
    goto err3;
    
  case 0:
    /* Child */
    close(in_pipe[1]);
    close(out_pipe[0]);
    if (dup2(in_pipe[0], 0) < 0) {
      perror("Cannot set pipe to stdin");
      exit(1);
    }
    if (dup2(out_pipe[1], 1) < 0) {
      perror("Cannot set pipe to stdout");
      exit(1);
    }
    if (SetSearchEnv(search) < 0)
      exit(1);
    
    execvp("CuraEngine", args->a);
    perror("Could not exec CuraEngine");
    exit(1);
    
  default:
    /* Parent */
    close(in_pipe[0]);
    close(out_pipe[1]);

    if (stdin_str && WriteStr(in_pipe[1], stdin_str) < 0)
      goto err5;
    
    close(in_pipe[1]);

    if (stdout_os && ReadToStream(out_pipe[0], stdout_os) < 0)
      goto err4;
    
    close(out_pipe[0]);
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      return -1;
    
    return 0;
  }
  
 err5:
  close(in_pipe[1]);
 err4:
  close(out_pipe[0]);
  waitpid(pid, NULL, 0);
  return -1;
  
 err3:
  close(out_pipe[0]);
  close(out_pipe[1]);
 err2:
  close(in_pipe[0]);
  close(in_pipe[1]);
 err:
  return -1;
}

static int Slice(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_file, const char *model_str) {
  struct ps_value_t *dflt;
  struct ps_context_t *ctx;
  struct args_t args;

  if ((dflt = PS_GetDefaults(ps)) == NULL)
    goto err;

  if ((ctx = PS_NewCtx(settings, dflt)) == NULL)
    goto err2;
  
  PS_FreeValue(dflt);
  dflt = NULL;
  
  if (PS_EvalAll(ps, ctx) < 0)
    goto err3;
  
  if (InitArgs(&args) < 0)
    goto err3;

  if (BuildArgs(&args, PS_GetPrinter(ps), PS_CtxGetValues(ctx), model_file) < 0)
    goto err4;
  
  if (ExecArgs(&args, model_str, gcode, PS_GetSearch(ps)) < 0)
    goto err4;
  
  return 0;

 err4:
  DestroyArgs(&args);
 err3:
  PS_FreeCtx(ctx);
 err2:
  PS_FreeValue(dflt);
 err:
  return -1;
}

int PS_SliceFile(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_file) {
  return Slice(gcode, ps, settings, model_file, NULL);
}

int PS_SliceStr(struct ps_ostream_t *gcode, const struct ps_value_t *ps, const struct ps_value_t *settings, const char *model_str) {
  return Slice(gcode, ps, settings, NULL, model_str);
}
