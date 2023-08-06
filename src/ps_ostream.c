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

#include <stdarg.h>
#include <string.h>

#include "ps_ostream.h"

struct ps_ostream_t {
  int is_file;
  
  char *str;
  size_t len;
  size_t alloc;
  
  FILE *file;
};

#define INIT_STR_SZ 256

struct ps_ostream_t *PS_NewFileOStream(FILE *out) {
  struct ps_ostream_t *os;

  if ((os = malloc(sizeof(*os))) == NULL) {
    perror("Cannot allocate memory for file ps_ostream");
    goto err;
  }
  memset(os, 0, sizeof(*os));

  os->is_file = 1;
  os->file = out;

  return os;
  
 err:
  return NULL;
}

struct ps_ostream_t *PS_NewStrOStream(void) {
  struct ps_ostream_t *os;

  if ((os = malloc(sizeof(*os))) == NULL) {
    perror("Cannot allocate memory for string ps_ostream");
    goto err;
  }
  memset(os, 0, sizeof(*os));

  if ((os->str = malloc(INIT_STR_SZ)) == NULL) {
    perror("Cannot allocate memory for string ps_ostream string");
    goto err2;
  }

  os->str[0] = '\0';
  os->alloc = INIT_STR_SZ;
  
  return os;

 err2:
  free(os);
 err:
  return NULL;
}

void PS_OStreamReset(struct ps_ostream_t *os) {
  os->len = 0;
}

void PS_FreeOStream(struct ps_ostream_t *os) {
  free(os->str);
  free(os);
}

int EnsureSpace(struct ps_ostream_t *os, size_t len) {
  size_t new_alloc;
  char *nstr;
  
  if (os->is_file)
    return 0;
  
  if (os->len + len < os->alloc)
    return 0;

  new_alloc = os->alloc;
  while (os->len + len >= new_alloc) {
    new_alloc <<= 1;
    if (new_alloc < os->alloc)
      return -1;
  }
  
  if ((nstr = realloc(os->str, new_alloc)) == NULL)
    return -1;

  os->str = nstr;
  os->alloc = new_alloc;

  return 1;
}

ssize_t PS_WriteBuf(struct ps_ostream_t *os, const void *buf, size_t len) {
  if (os->is_file) {
    return fwrite(buf, 1, len, os->file);
  }

  if (EnsureSpace(os, len) < 0)
    return -1;
  
  memcpy(os->str + os->len, buf, len);
  os->len += len;
  os->str[os->len] = '\0';

  return len;
}

int PS_WriteStr(struct ps_ostream_t *os, const char *str) {
  if (os->is_file) {
    return fputs(str, os->file);
  }

  return PS_WriteBuf(os, str, strlen(str));
}

int PS_WriteChar(struct ps_ostream_t *os, const char c) {
  if (os->is_file) {
    return fputc(c, os->file);
  }

  if (EnsureSpace(os, 1) < 0)
    return -1;
  
  os->str[os->len++] = c;
  os->str[os->len] = '\0';

  return c;
}

int PS_Printf(struct ps_ostream_t *os, const char *format, ...) {
  va_list ap;
  int len, ret;

  va_start(ap, format);

  if (os->is_file) {
    return vfprintf(os->file, format, ap);
  }
  
  if ((len = vsnprintf(os->str + os->len, os->alloc - os->len - 1, format, ap)) < 0)
    return -1;
  
  if ((ret = EnsureSpace(os, len)) < 0) {
    os->str[os->len] = '\0';
    return -1;
  }
  
  if (ret)
    vsnprintf(os->str + os->len, os->alloc - os->len - 1, format, ap);
  
  os->len += len;
  os->str[os->len] = '\0';

  return len;
}

static char HexDigit(char val) {
  if (val < 10)
    return '0' + val;

  return 'A' + val - 10;
}

ssize_t PS_WriteJsonBuf(struct ps_ostream_t *os, const char *str, size_t len, int include_quotes) {
  size_t count, bytes;
  char buf[4096], *cur, *end, ch;

  bytes = 0;
  end = buf + sizeof(buf) - 10;
  cur = buf;
  if (include_quotes)
    *cur++ = '"';
  for (count = 0; count < len; count++) {
    ch = str[count];
    switch (ch) {
    case '"': *cur++ = '\\'; *cur++ = '"'; break;
    case '/': *cur++ = '\\'; *cur++ = '/'; break;
    case '\\': *cur++ = '\\'; *cur++ = '\\'; break;
    case '\b': *cur++ = '\\'; *cur++ = 'b'; break;
    case '\f': *cur++ = '\\'; *cur++ = 'f'; break;
    case '\n': *cur++ = '\\'; *cur++ = 'n'; break;
    case '\r': *cur++ = '\\'; *cur++ = 'r'; break;
    case '\t': *cur++ = '\\'; *cur++ = 't'; break;
    default:
      if (ch <= 31) {
	*cur++ = '\\';
	*cur++ = 'u';
	*cur++ = '0';
	*cur++ = '0';
	*cur++ = HexDigit(ch >> 4);
	*cur++ = HexDigit(ch & 0xF);
      } else {
	*cur++ = ch;
      }
    }
    
    if (cur >= end) {
      bytes += cur - buf;
      if (PS_WriteBuf(os, buf, cur - buf) < 0)
	return -1;
      cur = buf;
    }
  }
  if (include_quotes)
    *cur++ = '"';
  bytes += cur - buf;
  if (PS_WriteBuf(os, buf, cur - buf) < 0)
    return -1;
  return bytes;
}

ssize_t PS_WriteJsonStr(struct ps_ostream_t *os, const char *str, int include_quotes) {
  return PS_WriteJsonBuf(os, str, strlen(str), include_quotes);
}

void PS_CloseOStream(struct ps_ostream_t *os) {
  fclose(os->file);
}

const char *PS_OStreamContents(struct ps_ostream_t *os) {
  return os->str;
}

size_t PS_OStreamLength(struct ps_ostream_t *os) {
  return os->len;
}
