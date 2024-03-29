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

#ifndef PS_OSTREAM_H
#define PS_OSTREAM_H

struct ps_ostream_t;

struct ps_ostream_t *PS_NewFileOStream(FILE *out);
struct ps_ostream_t *PS_NewStrOStream(void);
void PS_OStreamReset(struct ps_ostream_t *os);
void PS_FreeOStream(struct ps_ostream_t *os);

ssize_t PS_WriteBuf(struct ps_ostream_t *os, const void *buf, size_t len);
int PS_WriteStr(struct ps_ostream_t *os, const char *str);
int PS_WriteChar(struct ps_ostream_t *os, const char c);
int PS_Printf(struct ps_ostream_t *os, const char *format, ...);
ssize_t PS_WriteJsonBuf(struct ps_ostream_t *os, const char *buf, size_t len, int include_quotes);
ssize_t PS_WriteJsonStr(struct ps_ostream_t *os, const char *str, int include_quotes);

void PS_CloseOStream(struct ps_ostream_t *os);
const char *PS_OStreamContents(struct ps_ostream_t *os);
size_t PS_OStreamLength(struct ps_ostream_t *os);

#endif
