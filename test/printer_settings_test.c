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

#include "printer_settings.h"
#include "ps_ostream.h"
#include "ps_slice.h"

#ifdef WIN32
#define PSIZE "I"
#else
#define PSIZE "z"
#endif

int main(void) {
  struct ps_value_t *ps, *search, *ext, *set;
  struct ps_ostream_t *os, *stl;
  FILE *file;
  char buf[4096];
  size_t len;

  if ((search = PS_NewList()) == NULL)
    exit(1);
  if ((PS_AppendToList(search, PS_NewString("/usr/share/cura/resources/definitions"))) < 0)
    exit(1);
  if ((PS_AppendToList(search, PS_NewString("/usr/share/cura/resources/extruders"))) < 0)
    exit(1);

  if ((ps = PS_New("test.def.json", search)) == NULL)
    exit(1);

  if ((os = PS_NewFileOStream(stdout)) == NULL)
    exit(1);

  if ((ext = PS_ListExtruders(ps)) == NULL)
    exit(1);
  printf("Num extruders = %" PSIZE "u: ", PS_ItemCount(ext) - 1);
  PS_WriteValue(os, ext);
  printf("\n");
  
  /* PS_WriteValue(os, PS_GetDefaults(ps)); */
  /*PS_WriteValue(os, ps); */
  /* printf("\n"); */

  if ((set = PS_BlankSettings(ps)) == NULL)
    exit(1);
  
  if (PS_AddSetting(set, NULL, "material_diameter", PS_NewFloat(1.75)) < 0)
    exit(1);
  if (PS_AddSetting(set, NULL, "machine_nozzle_size", PS_NewFloat(0.4)) < 0)
    exit(1);
  if (PS_AddSetting(set, NULL, "layer_height", PS_NewFloat(0.2)) < 0)
    exit(1);
  if (PS_AddSetting(set, NULL, "line_width", PS_NewFloat(0.36)) < 0)
    exit(1);
  if (PS_AddSetting(set, NULL, "speed_print", PS_NewFloat(60)) < 0)
    exit(1);
  if (PS_AddSetting(set, NULL, "infill_sparse_density", PS_NewFloat(20)) < 0)
    exit(1);
  if (PS_AddSetting(set, NULL, "extruders_enabled_count", PS_NewInteger(1)) < 0)
    exit(1);
  
  if (PS_AddSetting(set, "0", "material_diameter", PS_NewFloat(1.75)) < 0)
    exit(1);  
  if (PS_AddSetting(set, "0", "machine_nozzle_size", PS_NewFloat(0.4)) < 0)
    exit(1);
  
  if ((stl = PS_NewStrOStream()) == NULL)
    exit(1);

  if ((file = fopen("test_widget.stl", "r")) == NULL)
    exit(1);

  while (!feof(file)) {
    len = fread(buf, 1, sizeof(buf), file);
    
    if (ferror(file))
      exit(1);
    
    if (PS_WriteBuf(stl, buf, len) < 0)
      exit(1);
  }
  
  //PS_SliceFile(os, ps, set, "test_widget.stl");
  PS_SliceStr(os, ps, set, PS_OStreamContents(stl), PS_OStreamLength(stl));
  PS_FreeValue(set);
  
  return 0;
}
