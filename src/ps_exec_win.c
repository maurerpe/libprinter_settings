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

#include <windows.h>

#include "ps_exec.h"

#define DWORD_MAX 4294967295

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static char Base64Encode(uint8_t val) {
  val &= 0x3F;
  
  if (val < 26)
    return 'A' + val;
  if (val < 52)
    return 'a' + val - 26;
  if (val < 62)
    return '0' + val - 52;
  if (val == 62)
    return '+';
  return '_';
}

static HANDLE Make_Temp_File(char *name_out, DWORD mode, const char *prefix, const char *ext) {
  HANDLE fh;
  char dir[MAX_PATH + 1];
  uint64_t ticks, val;
  size_t start;
  int count;
  
  if (!GetTempPathA(sizeof(dir), dir)) {
    fprintf(stderr, "Could not get location of temp folder\n");
    return INVALID_HANDLE_VALUE;
  }
  
  if (snprintf(name_out, MAX_PATH + 1, "%s%sXXXXXX.%s", dir, prefix, ext) > MAX_PATH) {
    fprintf(stderr, "Temp filename too long\n");
    return INVALID_HANDLE_VALUE;
  }
  
  ticks = GetTickCount64();
  start = strlen(name_out) - strlen(ext) - 2;
  
  while (1) {
    ticks++;
    
    val = ticks;
    for (count = 0; count < 6; count++) {
      name_out[start - count] = Base64Encode(val);
      val >>= 6;
    }
    
    if ((fh = CreateFile(name_out, mode, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_EXISTS)
	continue;
      fprintf(stderr, "Could not open temp file\n");
      return INVALID_HANDLE_VALUE;
    }

    break;
  }
  
  return fh;
}

char *PS_WriteToTempFile(const char *model_str, size_t len) {
  HANDLE fh;
  char *filename;
  DWORD count = 0;
  
  if ((filename = malloc(MAX_PATH + 1)) == NULL)
    goto err;
  
  if ((fh = Make_Temp_File(filename, GENERIC_WRITE, "ps", ".stl")) == INVALID_HANDLE_VALUE)
    goto err2;
  
  while (len > 0) {
    if (!WriteFile(fh, model_str, MIN(len, DWORD_MAX), &count, NULL)) {
      fprintf(stderr, "Cannot write to temp file\n");
      goto err3;
    }
  
    model_str += count;
    len -= count;
  }
  CloseHandle(fh);
  
  return filename;

 err3:
  CloseHandle(fh);
  PS_DeleteFile(filename);
 err2:
  free(filename);
 err:
  return NULL;
}

int PS_DeleteFile(const char *filename) {
  if (!DeleteFile(filename)) {
    fprintf(stderr, "Could not delete file\n");
    return -1;
  }

  return 0;
}

static int WriteStr(HANDLE fd, const char *str) {
  size_t len;
  DWORD count;

  len = strlen(str);
  while (len > 0) {
    if (!WriteFile(fd, str, MIN(len, DWORD_MAX), &count, NULL)) {
      fprintf(stderr, "Cannot write to pipe\n");
      return -1;
    }
  
    str += count;
    len -= count;
  }
  
  return 0;
}

static int ReadToStream(HANDLE fd, struct ps_ostream_t *os) {
  char buf[4096];
  DWORD count;
  
  while (1) {
    if (!ReadFile(fd, buf, sizeof(buf), &count, NULL)) {
      fprintf(stderr, "Cannot read from file\n");
      return -1;
    }
    
    if (count == 0)
      return 0;
    
    if (PS_WriteBuf(os, buf, count) < 0)
      return -1;
  }
}

struct ps_out_file_t {
  HANDLE fh;
  char filename[MAX_PATH + 1];
};

struct ps_out_file_t *PS_OutFile_New(void) {
  struct ps_out_file_t *of;
  char dir[MAX_PATH + 1];

  if ((of = malloc(sizeof(*of))) == NULL)
    goto err;
  memset(of, 0, sizeof(*of));
  
  if ((of->fh = Make_Temp_File(of->filename, GENERIC_READ, "ps", "gcd")) == INVALID_HANDLE_VALUE)
    goto err2;
  
  return of;

 err2:
  free(of);
 err:
  return NULL;
}

void PS_OutFile_Free(struct ps_out_file_t *of) {
  if (of == NULL)
    return;

  CloseHandle(of->fh);
  PS_DeleteFile(of->filename);
  free(of);
}

const char *PS_OutFile_GetName(const struct ps_out_file_t *of) {
  return of->filename;
}

int PS_OutFile_ReadToStream(struct ps_out_file_t *of, struct ps_ostream_t *os) {
  return ReadToStream(of->fh, os);
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
    if (!is_first && PS_WriteChar(os, ';') < 0)
	goto err3;
    is_first = 0;
    
    if (PS_WriteStr(os, PS_GetString(PS_ValueIteratorData(vi))) < 0)
      goto err3;
  }

  printf("Setting " PS_ENV_VAR "=%s\n", PS_OStreamContents(os));
  if (!SetEnvironmentVariable(PS_ENV_VAR, PS_OStreamContents(os))) {
    fprintf(stderr, "Could not set " PS_ENV_VAR " environment variable\n");
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

static int GetCuraPath(char *path, const char *suffix) {
  char dir[MAX_PATH + 1], search[MAX_PATH + 1];
  WIN32_FIND_DATAA found;
  
  if (!GetEnvironmentVariable("ProgramW6432", dir, sizeof(dir))) {
    fprintf(stderr, "Could not get ProgramW6432 environment variable\n");
    return -1;
  }
  
  if (snprintf(search, sizeof(search), "%s\\UltiMaker Cura*", dir) >= MAX_PATH) {
    fprintf(stderr, "Program Files folder path too long\n");
    return -1;
  }
  
  if (!FindFirstFileA(search, &found)) {
    fprintf(stderr, "Could not find Cura installation\n");
    return -1;
  }
  
  if (snprintf(path, MAX_PATH, "%s\\%s\\%s", dir, found.cFileName, suffix) >= MAX_PATH) {
    fprintf(stderr, "Cura path too long\n");
    return -1;
  }
  
  printf("Using %s\n", path);
  
  return 0;
}

struct ps_value_t *PS_GetDefaultSearch(void) {
  struct ps_value_t *search, *str;
  char path[MAX_PATH + 1];
  
  if ((search = PS_NewList()) == NULL)
    goto err;
  if (GetCuraPath(path, "share\\cura\\resources\\definitions") < 0)
    goto err2;
  if ((str = PS_NewString(path)) == NULL)
    goto err2;
  if (PS_AppendToList(search, str) < 0)
    goto err3;
  if (GetCuraPath(path, "share\\cura\\resources\\extruders") < 0)
    goto err2;
  if ((str = PS_NewString(path)) == NULL)
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

int PS_ExecArgs(char * const *args, const char *stdin_str, struct ps_ostream_t *stdout_os, const struct ps_value_t *search) {
  SECURITY_ATTRIBUTES saAttr;
  HANDLE in_pipe_rd, in_pipe_wr, out_pipe_rd, out_pipe_wr;
  PROCESS_INFORMATION proc_info;
  STARTUPINFO startup_info;
  struct ps_ostream_t *os;
  char *cmdline, cura_path[MAX_PATH + 1];
  int init;
  
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;
  
  if (!CreatePipe(&in_pipe_rd, &in_pipe_wr, &saAttr, 0)) {
    fprintf(stderr, "Could not create in pipe\n");
    goto err;
  }
  
  if (!SetHandleInformation(in_pipe_wr, HANDLE_FLAG_INHERIT, 0)) {
    fprintf(stderr, "Unable to make in_pipe_wr uninherited\n");
    goto err2;
  }
  
  memset(&proc_info, 0, sizeof(proc_info));
  memset(&startup_info, 0, sizeof(startup_info));

  startup_info.cb = sizeof(STARTUPINFO);
  startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup_info.hStdInput = in_pipe_rd;
  startup_info.dwFlags |= STARTF_USESTDHANDLES;

  if (stdout_os) {
    if (!CreatePipe(&out_pipe_rd, &out_pipe_wr, &saAttr, 0)) {
      fprintf(stderr, "Could not create out pipe\n");
      goto err2;
    }
    
    if (!SetHandleInformation(out_pipe_rd, HANDLE_FLAG_INHERIT, 0)) {
      fprintf(stderr, "Unable to make out_pipe_rd uninherited\n");
      goto err3;
    }

    startup_info.hStdOutput = out_pipe_wr;
  }
  
  if (GetCuraPath(cura_path, "CuraEngine.exe") < 0)
    goto err3;
  
  if ((os = PS_NewStrOStream()) == NULL)
    goto err3;
  
  init = 0;
  if (PS_WriteStr(os, "\"") < 0)
    goto err4;
  while (*args) {
    if (init && PS_WriteStr(os, "\" \"") < 0)
      goto err4;
    init = 1;
    if (PS_WriteStr(os, *args) < 0)
      goto err4;
    args++;
  }
  if (PS_WriteStr(os, "\"") < 0)
    goto err4;

  if (SetSearchEnv(search) < 0)
    goto err4;
  
  if ((cmdline = strdup(PS_OStreamContents(os))) == NULL)
    goto err5;
  
  if (!CreateProcessA(cura_path, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &startup_info, &proc_info)) {
    fprintf(stderr, "Could not create child process\n");
    goto err5;
  }

  free(cmdline);
  PS_FreeOStream(os);
  CloseHandle(proc_info.hThread);
  stdout_os && CloseHandle(out_pipe_wr);
  CloseHandle(in_pipe_rd);
  
  if (stdin_str && WriteStr(in_pipe_wr, stdin_str) < 0)
    goto err7;
  CloseHandle(in_pipe_wr);

  if (stdout_os) {
    if (ReadToStream(out_pipe_rd, stdout_os) < 0)
      goto err6;
    CloseHandle(out_pipe_rd);
  }
  
  WaitForSingleObject(proc_info.hProcess, INFINITE);
  CloseHandle(proc_info.hProcess);
  return 0;

 err7:
  CloseHandle(in_pipe_wr);
 err6:
  stdout_os && CloseHandle(out_pipe_rd);
  WaitForSingleObject(proc_info.hProcess, INFINITE);
  CloseHandle(proc_info.hProcess);
  return -1;

 err5:
  free(cmdline);
 err4:
  PS_FreeOStream(os);
 err3:
  stdout_os && CloseHandle(out_pipe_rd);
  stdout_os && CloseHandle(out_pipe_wr);
 err2:
  CloseHandle(in_pipe_rd);
  CloseHandle(in_pipe_wr);
 err:
  return -1;
}
