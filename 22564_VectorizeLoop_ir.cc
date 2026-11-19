// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 64 * 16 * 1 * 30]
produce data_vec {
  parallel (C.h.fused, 0, 1024) {
    for (w, 0, 30) {
      data_vec[((C.h.fused*30) + w)] = placeholder[((C.h.fused*30) + w)]
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
  parallel (c.outer.h.outer.fused, 0, 16) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 30 * 1]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 30 * 1]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 30) {
          conv.global[ow.c.inner.init] = (int64)0
        }
        for (ic.outer, 0, 64) {
          unrolled (ow.c.inner, 0, 30) {
            conv.global[ow.c.inner] = (conv.global[ow.c.inner] + (int64(data_vec[(((ic.outer*480) + (c.outer.h.outer.fused*30)) + ow.c.inner)])*int64(kernel_vec[ic.outer])))
          }
        }
      }
      for (ow, 0, 30) {
        conv[ow] = conv.global[ow]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[((c.outer.h.outer.fused*30) + w.inner)] = conv[w.inner]
    }
  }
}
