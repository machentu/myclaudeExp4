// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 30720]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 64]
produce data_vec {
  parallel (C.h.fused, 0, 1024) {
    for (w, 0, 30) {
      data_vec[((C.h.fused*30) + w)] = placeholder[((C.h.fused*30) + w)]
    }
  }
}
produce kernel_vec {
  for (CI, 0, 64) {
    kernel_vec[CI] = placeholder[CI]
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 16) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 30]
    produce conv {
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
        conv.global[ow] = conv.global[ow]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[((c.outer.h.outer.fused*30) + w.inner)] = conv.global[w.inner]
    }
  }
}
