// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 64 * 8 * 1 * 15]
produce data_vec {
  parallel (C.h.fused, 0, 512) {
    for (w, 0, 15) {
      data_vec[((C.h.fused*15) + w)] = placeholder[((C.h.fused*15) + w)]
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
  parallel (c.outer.h.outer.fused, 0, 4) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 2 * 15 * 1]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 2 * 15 * 1]
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 2) {
          unrolled (ow.c.inner.init, 0, 15) {
            conv.global[((oh.c.inner.init*15) + ow.c.inner.init)] = (int64)0
          }
        }
        for (ic.outer, 0, 64) {
          unrolled (oh.c.inner, 0, 2) {
            unrolled (ow.c.inner, 0, 15) {
              conv.global[((oh.c.inner*15) + ow.c.inner)] = (conv.global[((oh.c.inner*15) + ow.c.inner)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (oh.c.inner*15)) + ow.c.inner)])*int64(kernel_vec[ic.outer])))
            }
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 15) {
          conv[((oh.inner*15) + ow)] = conv.global[((oh.inner*15) + ow)]
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[(((c.outer.h.outer.fused*30) + (h.inner*15)) + w.inner)] = conv[((h.inner*15) + w.inner)]
      }
    }
  }
}
