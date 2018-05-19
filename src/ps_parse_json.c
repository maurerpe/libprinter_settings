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

#include <ctype.h>
#include <string.h>

#include "ps_value.h"
#include "ps_ostream.h"

#define BUF_SZ 4096

struct buffer {
  FILE *in;
  char *loc;
  char *end;
  char *buf;
  size_t line;
  char *line_start;
  struct ps_ostream_t *os;
};

static struct ps_value_t *ParseValue(struct buffer *buf);

#ifdef WIN32
#define PSIZE "I"
#else
#define PSIZE "z"
#endif

#define PRINT_ERR \
  fprintf(stderr, "Error parsing json at %" PSIZE "u:%" PSIZE "u\n", buf->line + 1, buf->loc - buf->line_start)

static size_t FillBuf(struct buffer *buf) {
  size_t num;
  
  buf->line_start -= buf->loc - buf->buf;
  
  if (buf->in == NULL)
    num = 0;
  else
    num = fread(buf->buf, 1, BUF_SZ, buf->in);
  buf->loc = buf->buf;
  buf->end = buf->buf + num;

  if (num == 0 && buf->in && ferror(buf->in)) {
    PRINT_ERR;
    fprintf(stderr, "Error reading json file\n");
  }
  
  return num;
}

#define ADV(eof_statements)			\
  do {						\
    if (++buf->loc >= buf->end)			\
      if (FillBuf(buf) == 0) {			\
	eof_statements;				\
      }						\
    if (*buf->loc == '\n') {			\
      buf->line++;				\
      buf->line_start = buf->loc;		\
    }						\
  } while (0)

#define IS_EOF (buf->end == buf->buf)

#define SKIP_WHITE(eof_statements)		\
  do {						\
    while (!IS_EOF && isspace(*buf->loc))	\
      ADV(eof_statements);			\
  } while (0)

static struct ps_value_t *ParseBare(struct buffer *buf) {
  char ch, temp[1024], *cur = temp, *end = temp + sizeof(temp) - 5;
  const char *str, *strend, *stop;
  int is_float = 0;
  double f;
  long long i;
  struct ps_value_t *v;

  PS_OStreamReset(buf->os);
  
  while (1) {
    ch = *buf->loc;
    if (!isalnum(ch) && ch != '.' && ch != '-' && ch != '+')
      break;
    
    if (cur >= end) {
      PS_WriteBuf(buf->os, temp, cur - temp);
      cur = temp;
    }
    
    if (ch == '.' || ch == 'e' || ch == 'E')
      is_float = 1;
    
    *cur++ = ch;
    
    ADV(goto loop_done);
  }
 loop_done:

  PS_WriteBuf(buf->os, temp, cur - temp);
  str = PS_OStreamContents(buf->os);
  strend = str + PS_OStreamLength(buf->os);
  if (strcmp(str, "null") == 0)
    return PS_NewNull();

  if (strcmp(str, "false") == 0)
    return PS_NewBoolean(0);

  if (strcmp(str, "true") == 0)
    return PS_NewBoolean(1);
  
  if (is_float) {
    f = strtod(str, (char **) &stop);
    if (stop != strend) {
      PRINT_ERR;
      fprintf(stderr, "Unexpected garbage after number");
      return NULL;
    }
    if ((v = PS_NewFloat(f)) == NULL)
      return NULL;
    return v;
  }

  i = strtoll(str, (char **) &stop, 0);
  if (stop != strend) {
    PRINT_ERR;
    fprintf(stderr, "Unexpected garbage after number");
    return NULL;
  }
  if ((v = PS_NewInteger(i)) == NULL)
    return NULL;
  return v;
}

static char *ParseHex4(struct buffer *buf, char *cur) {
  char ch;
  int count;
  uint32_t val = 0;

  for (count = 0; count < 4; count++) {
    ADV(goto eof);
    ch = *buf->loc;
    val <<= 4;
    if (ch >= '0' && ch <= '9')
      val |= ch - '0';
    else if (ch >= 'A' && ch <= 'F')
      val |= ch - 'A' + 10;
    else if (ch >= 'a' && ch <= 'f')
      val |= ch - 'a' + 10;
    else {
      PRINT_ERR;
      fprintf(stderr, "Invalid hex digit in \\u string escape\n");
      return NULL;
    }
  }

  /* Encode as utf-8 */
  if (val <= 0x7F) {
    *cur++ = val;
  } else if (val <= 0x7FF) {
    *cur++ = 0xC0 | val >> 6;
    *cur++ = 0x80 | (val & 0x3F);
  } else {
    *cur++ = 0xE0 | val >> 12;
    *cur++ = 0x80 | ((val >> 6) & 0x3F);
    *cur++ = 0x80 | (val & 0x3F);
  }
  
  return cur;
  
 eof:
  fprintf(stderr, "Unexpected in of file in \\u string escape in json file\n");
  return NULL;
}

static int ParseString(struct buffer *buf, struct ps_ostream_t *os) {
  char temp[1024], *cur = temp, *end = temp + sizeof(temp) - 5;

  PS_OStreamReset(os);
  
  while (1) {
    ADV(goto eof);
    
    if (*buf->loc == '"')
      break;
    
    if (cur >= end) {
      PS_WriteBuf(os, temp, cur - temp);
      cur = temp;
    }
    
    if (*buf->loc == '\\') {
      ADV(goto eof);
      switch (*buf->loc) {
      case 'b': *cur++ = '\b'; break;
      case 'f': *cur++ = '\f'; break;
      case 'n': *cur++ = '\n'; break;
      case 'r': *cur++ = '\r'; break;
      case 't': *cur++ = '\t'; break;
      case 'u':
	if ((cur = ParseHex4(buf, cur)) == NULL)
	  return -1;
	break;
      default: *cur++ = *buf->loc; break;
      }
    } else {
      *cur++ = *buf->loc;
    }
  }
  
  PS_WriteBuf(os, temp, cur - temp);
  ADV(;);
  return 0;
  
 eof:
  fprintf(stderr, "Unexpected eof in json string\n");
  return -1;
}

static struct ps_value_t *ParseList(struct buffer *buf) {
  struct ps_value_t *v, *sub;
  
  if ((v = PS_NewList()) == NULL)
    goto err;
  
  ADV(goto eof);
  SKIP_WHITE(goto eof);
  if (*buf->loc != ']') {
    while (1) {
      if ((sub = ParseValue(buf)) == NULL)
	goto err2;
      if ((PS_AppendToList(v, sub)) < 0) {
	PS_FreeValue(sub);
	goto err2;
      }
      SKIP_WHITE(goto eof);
      if (*buf->loc == ']')
	break;
      if (*buf->loc != ',') {
	PRINT_ERR;
	fprintf(stderr, "Expected either ',' or ']'\n");
	goto err2;
      }
      ADV(goto eof);
    }
  }

  ADV(;);
  return v;
  
 eof:
  fprintf(stderr, "Unexpected eof parsing list\n");
 err2:
  PS_FreeValue(v);
 err:
  return NULL;
}

static struct ps_value_t *ParseObject(struct buffer *buf) {
  struct ps_value_t *v = NULL, *sub;
  struct ps_ostream_t *name;

  if ((v = PS_NewObject()) == NULL)
    goto err;
  if ((name = PS_NewStrOStream()) == NULL)
    goto err2;
  
  ADV(goto eof);
  SKIP_WHITE(goto eof);
  if (*buf->loc != '}') {
    while (1) {
      SKIP_WHITE(goto eof);
      if (*buf->loc != '"') {
	PRINT_ERR;
	fprintf(stderr, "Expected '\"' to start member name, found '%c'\n", *buf->loc);
	goto err3;
      }
      if (ParseString(buf, name) < 0)
	goto err3;
      
      SKIP_WHITE(goto eof);
      if (*buf->loc != ':') {
	PRINT_ERR;
	fprintf(stderr, "Expected ':' to delineate value, found '%c'\n", *buf->loc);
	goto err3;
      }
      ADV(goto eof);
      
      if ((sub = ParseValue(buf)) == NULL)
	goto err3;
      if ((PS_AddMember(v, PS_OStreamContents(name), sub)) < 0) {
	PS_FreeValue(sub);
	goto err3;
      }
      SKIP_WHITE(goto eof);
      if (*buf->loc == '}')
	break;
      if (*buf->loc != ',') {
	PRINT_ERR;
	fprintf(stderr, "Expected either ',' or '}', found '%c'\n", *buf->loc);
	goto err3;
      }
      ADV(goto eof);
    }
  }
  
  PS_FreeOStream(name);
  ADV(;);
  return v;

 eof:
  fprintf(stderr, "Unexpected eof parsing object\n");
 err3:
  PS_FreeOStream(name);
 err2:
  PS_FreeValue(v);
 err:
  return NULL;
}

static struct ps_value_t *ParseValue(struct buffer *buf) {
  SKIP_WHITE(goto eof);  
  if (*buf->loc == '"') {
    if (ParseString(buf, buf->os) < 0)
      return NULL;
    return PS_NewString(PS_OStreamContents(buf->os));
  }

  if (*buf->loc == '[')
    return ParseList(buf);

  if (*buf->loc == '{')
    return ParseObject(buf);
  
  return ParseBare(buf);
  
 eof:
  fprintf(stderr, "Unexpected eof parsing value\n");
  return NULL;
}

struct ps_value_t *PS_ParseJsonFile(FILE *in) {
  struct buffer buf;
  struct ps_value_t *v = NULL;
  
  if (in == NULL)
    goto err;

  memset(&buf, 0, sizeof(buf));
  
  if ((buf.buf = malloc(BUF_SZ)) == NULL) {
    fprintf(stderr, "Could not allocate memory for Json file buffer\n");
    goto err;
  }
  
  buf.line_start = buf.loc = buf.end = buf.buf;
  buf.in = in;
  if ((buf.os = PS_NewStrOStream()) == NULL)
    goto err2;
  
  FillBuf(&buf);
  v = ParseValue(&buf);
  
  /* Fall through */
  PS_FreeOStream(buf.os);
 err2:
  free(buf.buf);
 err:
  return v;
}

struct ps_value_t *PS_ParseJsonString(const char *str) {
  struct buffer buf;
  struct ps_value_t *v = NULL;
  
  if (str == NULL)
    goto err;

  memset(&buf, 0, sizeof(buf));
  
  buf.line_start = buf.loc = buf.buf = (char *) str;
  buf.end = buf.buf + strlen(str);
  buf.in = NULL;
  
  if ((buf.os = PS_NewStrOStream()) == NULL)
    goto err;
  
  v = ParseValue(&buf);
  
  /* Fall through */
  PS_FreeOStream(buf.os);
 err:
  return v;
}
