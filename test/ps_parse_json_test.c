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

#include "ps_parse_json.h"
#include "ps_ostream.h"

void TestStr(const char *str, struct ps_ostream_t *os) {
  struct ps_value_t *v;
  
  if ((v = PS_ParseJsonString(str)) == NULL) {
    fprintf(stderr, "Error parsing json string: '%s'\n", str);
    exit(1);
  }

  PS_OStreamReset(os);
  PS_WriteValue(os, v);
  
  printf("'%s' -> '%s'\n", str, PS_OStreamContents(os));

  PS_OStreamReset(os);
  PS_WriteValuePretty(os, v);  
  printf("%s\n", PS_OStreamContents(os));
  
  PS_FreeValue(v);
}

int main(void) {
  struct ps_ostream_t *os;
  
  if ((os = PS_NewStrOStream()) == NULL)
    exit(1);

  TestStr("null", os);
  TestStr("false", os);
  TestStr("-391e-3", os);
  TestStr("[\"list\",2,true]", os);
  TestStr("{\"name\": \"Bob\",\"number\":4,\"list\":[5,6,true,null,{},[]]}", os);
  TestStr("{\"#global\": {\"Bob\": 4,\"poly\":[[3,2],[2,1],[39,91]],\"list\":[5,6,true,null,{\"test\": 4, \"hi\":[]},[]]},\"0\": {\"material_diameter\":3.13}}", os);
}
