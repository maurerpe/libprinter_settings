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

#include "ps_exec.h"

struct ps_value_t *PS_GetDefaultSearch(void) {
  struct ps_value_t *search, *str;
  
  if ((search = PS_NewList()) == NULL)
    goto err;
  if ((str = PS_NewString("/usr/share/cura/resources/definitions")) == NULL)
    goto err2;
  if (PS_AppendToList(search, str) < 0)
    goto err3;
  if ((str = PS_NewString("/usr/share/cura/resources/extruders")) == NULL)
    goto err2;
  if (PS_AppendToList(search, str) < 0)
    goto err3;

  return search;

 err3:
  PS_FreeValue(str);
 err2:
  PS_FreeValue(search);
 err:
  return NULL;
}

char *PS_WriteToTempFile(const char *model_str, size_t len) {
  char *str;
  int fd;
  ssize_t count;
  
  if ((str = strdup("/tmp/printer_settings_XXXXXX.stl")) == NULL)
    goto err;
  
  if ((fd = mkstemps(str, 4)) < 0)
    goto err2;

  while (len > 0) {
    if ((count = write(fd, model_str, len)) < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	continue;
      perror("Cannot write to temp file");
      goto err3;
    }

    model_str += count;
    len -= count;
  }
  close(fd);
  
  return str;
  
 err3:
  close(fd);
 err2:
  free(str);
 err:
  return NULL;
}

int PS_DeleteFile(const char *filename) {
  if (unlink(filename) < 0) {
    perror("Could not delete file");
    return -1;
  }
  
  return 0;
}

static int WriteStr(int fd, const char *str) {
  size_t len;
  ssize_t count;
  
  len = strlen(str);
  while (len > 0) {
    if ((count = write(fd, str, len)) < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	continue;
      perror("Cannot write string to cura");
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

struct ps_out_file_t {
  int fd;
  char *filename;
};

struct ps_out_file_t *PS_OutFile_New(void) {
  struct ps_out_file_t *of;

  if ((of = malloc(sizeof(*of))) == NULL)
    goto err;

  if ((of->filename = strdup("/tmp/printer_settings_XXXXXX.gcode")) == NULL)
    goto err2;

  if ((of->fd = mkstemps(of->filename, 6)) < 0)
    goto err3;

  return of;

 err3:
  free(of->filename);
 err2:
  free(of);
 err:
  return NULL;
}

void PS_OutFile_Free(struct ps_out_file_t *of) {
  if (of == NULL)
    return;

  close(of->fd);
  PS_DeleteFile(of->filename);
  free(of->filename);
  free(of);
}

const char *PS_OutFile_GetName(const struct ps_out_file_t *of) {
  return of->filename;
}

int PS_OutFile_ReadToStream(struct ps_out_file_t *of, struct ps_ostream_t *os) {
  return ReadToStream(of->fd, os);
}

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
  fprintf(stderr, "Setting " PS_ENV_VAR "=%s\n", PS_OStreamContents(os));
#endif
  if (setenv(PS_ENV_VAR, PS_OStreamContents(os), 1) < 0) {
    perror("Could not set " PS_ENV_VAR " environment variable");
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

int PS_ExecArgs(char * const *args, const char *stdin_str, struct ps_ostream_t *stdout_os, const struct ps_value_t *search) {
  int in_pipe[2], out_pipe[2], status;
  pid_t pid;

  if (pipe(in_pipe) < 0)
    goto err;

  if (stdout_os && pipe(out_pipe) < 0)
    goto err2;
  
  switch ((pid = fork())) {
  case -1:
    goto err3;
    
  case 0:
    /* Child */
    close(in_pipe[1]);
    if (dup2(in_pipe[0], 0) < 0) {
      perror("Cannot set pipe to stdin");
      exit(1);
    }
    if (stdout_os) {
      close(out_pipe[0]);
      if (dup2(out_pipe[1], 1) < 0) {
	perror("Cannot set pipe to stdout");
	exit(1);
      }
    }
    if (SetSearchEnv(search) < 0)
      exit(1);
    
    execvp("CuraEngine", args);
    perror("Could not exec CuraEngine");
    exit(1);
    
  default:
    /* Parent */
    close(in_pipe[0]);
    if (stdout_os)
      close(out_pipe[1]);

    if (stdin_str && WriteStr(in_pipe[1], stdin_str) < 0)
      goto err5;
    
    close(in_pipe[1]);

    if (stdout_os) {
      if (ReadToStream(out_pipe[0], stdout_os) < 0)
	goto err4;
      close(out_pipe[0]);
    }
    
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      return -1;
    
    return 0;
  }
  
 err5:
  close(in_pipe[1]);
 err4:
  if (stdout_os)
    close(out_pipe[0]);
  waitpid(pid, NULL, 0);
  return -1;
  
 err3:
  if (stdout_os) {
    close(out_pipe[0]);
    close(out_pipe[1]);
  }
 err2:
  close(in_pipe[0]);
  close(in_pipe[1]);
 err:
  return -1;
}
