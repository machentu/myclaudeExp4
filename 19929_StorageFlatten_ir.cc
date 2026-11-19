// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 4 * 16 * 1 * 1 * 2520]
produce data_vec {
  for (n, 0, 4) {
    parallel (C.h.fused, 0, 16) {
      for (w, 0, 2520) {
        data_vec[((((n*16) + C.h.fused)*2520) + w)] = placeholder[((((n*16) + C.h.fused)*2520) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 1 * 16 * 1 * 1 * 1 * 1]
produce kernel_vec {
  for (CI, 0, 16) {
    kernel_vec[CI] = placeholder[CI]
  }
}
produce output_unpack {
  for (n, 0, 4) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 2520 * 1]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 2520 * 1]
      produce conv.global {
        for (ow.c.outer, 0, 84) {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[(ow.c.inner.init + (ow.c.outer*30))] = (int64)0
          }
          for (ic.outer, 0, 16) {
            unrolled (ow.c.inner, 0, 30) {
              conv.global[(ow.c.inner + (ow.c.outer*30))] = (conv.global[(ow.c.inner + (ow.c.outer*30))] + (int64(data_vec[((((n*16) + ic.outer)*2520) + (ow.c.inner + (ow.c.outer*30)))])*int64(kernel_vec[ic.outer])))
            }
          }
        }
      }
      for (ow, 0, 2520) {
        conv[ow] = conv.global[ow]
      }
    }
    for (w.outer, 0, 84) {
      for (w.inner, 0, 30) {
        output_unpack[((n*2520) + (w.inner + (w.outer*30)))] = conv[(w.inner + (w.outer*30))]
      }
    }
  }
}
