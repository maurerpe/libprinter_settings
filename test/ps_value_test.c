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

#include "ps_value.h"
#include "ps_ostream.h"

#define AddMember(type, arg)			\
  do {						\
    v = PS_New ## type (arg);			\
    PS_AddMember(obj, #type, v);		\
  } while (0)

#define AddElement(type, arg)			\
  do {						\
    v = PS_New ## type (arg);			\
    PS_AppendToList(list, v);			\
  } while (0)

int main(void) {
  struct ps_value_t *obj, *list, *v;
  struct ps_ostream_t *os;
  
  if ((obj = PS_NewObject()) == NULL)
    exit(1);
  
  AddMember(Null,);
  AddMember(Boolean, 1);
  AddMember(Integer, 391);
  AddMember(Float, -3.181);
  AddMember(String, "Test \"String\"");
  AddMember(Variable, "var");

  if ((list = PS_NewList()) == NULL)
    exit(1);
  
  AddElement(Null,);
  AddElement(Boolean, 0);
  AddElement(Integer, -391);
  AddElement(Float, 5e6);
  AddElement(String, "Test \"String\"");
  AddElement(Variable, "foo");
  PS_AddMember(obj, "list", list);
  
  os = PS_NewFileOStream(stdout);
  PS_WriteValue(os, obj);
  PS_FreeOStream(os);

  os = PS_NewStrOStream();
  PS_WriteValue(os, obj);
  printf("\n\n");
  puts(PS_OStreamContents(os));
  printf("\n");
}
