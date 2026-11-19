// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 64 * 32 * 1 * 60]
produce data_vec {
  parallel (C.h.fused, 0, 2048) {
    for (w, 0, 60) {
      data_vec[((C.h.fused*60) + w)] = placeholder[((C.h.fused*60) + w)]
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 1 * 64 * 1 * 1 * 1 * 1]
produce kernel_vec {
  for (CI, 0, 64) {
    kernel_vec[CI] = placeholder[CI]
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 32) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 1]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 60 * 1]
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[(ow.c.inner.init + (ow.c.outer*30))] = (int64)0
          }
          for (ic.outer, 0, 64) {
            unrolled (ow.c.inner, 0, 30) {
              conv.global[(ow.c.inner + (ow.c.outer*30))] = (conv.global[(ow.c.inner + (ow.c.outer*30))] + (int64(data_vec[((((ic.outer*32) + (c.outer.h.outer.fused % 32))*60) + (ow.c.inner + (ow.c.outer*30)))])*int64(kernel_vec[(((c.outer.h.outer.fused/32)*64) + ic.outer)])))
            }
          }
        }
      }
      for (ow, 0, 60) {
        conv[ow] = conv.global[ow]
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[((c.outer.h.outer.fused*60) + (w.inner + (w.outer*30)))] = conv[(w.inner + (w.outer*30))]
      }
    }
  }
}
