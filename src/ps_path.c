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

#include <string.h>

#include "ps_path.h"

#define PATHSEP '/'

struct ps_value_t *PS_PathFromString(const char *str) {
  struct ps_value_t *p, *v;
  const char *dir, *ext;

  if ((p = PS_NewObject()) == NULL)
    goto err;
  
  if ((dir = strrchr(str, PATHSEP)) == NULL)
    dir = str - 1;
  dir++;

  if ((ext = strchr(dir, '.')) == NULL)
    ext = dir + strlen(dir);

  if ((v = PS_NewStringLen(str, dir - str)) == NULL)
    goto err2;
  
  if ((PS_AddMember(p, "directory", v)) < 0)
    goto err3;
  
  if ((v = PS_NewStringLen(dir, ext - dir)) == NULL)
    goto err2;

  if ((PS_AddMember(p, "basename", v)) < 0)
    goto err3;
  
  if ((v = PS_NewString(ext)) == NULL)
    goto err2;

  if ((PS_AddMember(p, "extension", v)) < 0)
    goto err3;
  
  return p;
  
 err3:
  PS_FreeValue(v);
 err2:
  PS_FreeValue(p);
 err:
  return NULL;
}

struct ps_value_t *PS_PathToString(const struct ps_value_t *path) {
  struct ps_value_t *s;
  const struct ps_value_t *v;
  const char *str;
  
  if ((v = PS_GetMember(path, "directory", NULL)) == NULL ||
      (str = PS_GetString(v)) == NULL ||
      (s = PS_NewString(str)) == NULL)
    goto err;

  if ((v = PS_GetMember(path, "basename", NULL)) == NULL ||
      (str = PS_GetString(v)) == NULL ||
      PS_AppendToString(s, str) < 0)
    goto err2;
  
  if ((v = PS_GetMember(path, "extension", NULL)) == NULL ||
      (str = PS_GetString(v)) == NULL ||
      PS_AppendToString(s, str) < 0)
    goto err2;
  
  return s;

 err2:
  PS_FreeValue(s);
 err:
  return NULL;
}

int PS_IsPathAbsolute(const struct ps_value_t *path) {
  const struct ps_value_t *v;
  const char *str;
  
  if ((v = PS_GetMember(path, "directory", NULL)) == NULL ||
      (str = PS_GetString(v)) == NULL)
    return 0;

  return *str == PATHSEP;
}

static void EnsureSlash(struct ps_value_t *v) {
  const char *str;
  char aa[2];
  size_t len;

  if ((str = PS_GetString(v)) == NULL)
    return;

  if (*str == '\0')
    return;

  len = strlen(str);
  if (str[len - 1] == PATHSEP)
    return;
  
  aa[0] = PATHSEP;
  aa[1] = '\0';
  PS_AppendToString(v, aa);
}

FILE *PS_OpenSearch(const char *filename, const char *mode, const struct ps_value_t *default_ext, const struct ps_value_t *search, struct ps_value_t **final_out) {
  struct ps_value_t *p, *d, *s;
  const struct ps_value_t *v;
  const char *str, *dstr;
  FILE *ff;
  size_t count;

  if ((p = PS_PathFromString(filename)) == NULL)
    goto err;
  
  if ((v = PS_GetMember(p, "extension", NULL)) == NULL ||
      (str = PS_GetString(v)) == NULL)
    goto err2;
  
  if (*str == '\0' && default_ext) {
    if ((d = PS_CopyValue(default_ext)) == NULL)
      goto err2;
    if (PS_AddMember(p, "extension", d) < 0) {
      PS_FreeValue(d);
      goto err2;
    }
  }
  
  if ((s = PS_PathToString(p)) == NULL)
    goto err2;
  if ((str = PS_GetString(s)) == NULL)
    goto err3;
  
  ff = fopen(str, mode);
  
  PS_FreeValue(s);
  
  if (ff == NULL && search && !PS_IsPathAbsolute(p)) {
    count = 0;
    if ((v = PS_GetMember(p, "directory", NULL)) == NULL)
      goto err2;
    d = PS_CopyValue(v);
    if ((dstr = PS_GetString(d)) == NULL) {
      PS_FreeValue(d);
      goto err2;
    }
    while (ff == NULL) {
      if ((v = PS_GetItem(search, count++)) == NULL) {
	PS_FreeValue(d);
	goto err2;
      }
      s = PS_CopyValue(v);
      EnsureSlash(s);
      if (PS_AppendToString(s, dstr) < 0 ||
	  PS_AddMember(p, "directory", s) < 0) {
	PS_FreeValue(d);
	goto err3;
      }

      if ((s = PS_PathToString(p)) == NULL) {
	PS_FreeValue(d);
	goto err2;
      }
      if ((str = PS_GetString(s)) == NULL) {
	PS_FreeValue(d);
	goto err3;
      }
      
      ff = fopen(str, mode);
      PS_FreeValue(s);
    }
    PS_FreeValue(d);
  } else if (ff == NULL)
    goto err2;

  if (final_out)
    *final_out = p;
  else
    PS_FreeValue(p);
  
  return ff;
  
 err3:
  PS_FreeValue(s);
 err2:
  PS_FreeValue(p);
 err:
  return NULL;
}
