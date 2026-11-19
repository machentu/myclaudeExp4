// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 4 * 16 * 8 * 30]
produce data_vec {
  parallel (C.h.fused, 0, 64) {
    for (c, 0, 8) {
      for (w, 0, 30) {
        data_vec[((((C.h.fused*8) + c)*30) + w)] = placeholder[(((((((C.h.fused/16)*8) + c)*16) + (C.h.fused % 16))*30) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 4 * 4 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 4) {
    for (CI, 0, 4) {
      for (ci, 0, 8) {
        vectorized (co, 0, 8) {
          kernel_vec[((((((CO.h.fused*4) + CI)*8) + ci)*8) + co)] = placeholder[((((CO.h.fused*8) + co)*32) + ((CI*8) + ci))]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 64) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 30 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 30 * 8]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 30) {
          vectorized (oc_block.c.init, 0, 8) {
            conv.global[((ow.c.inner.init*8) + oc_block.c.init)] = (int64)0
          }
        }
        for (ic.outer, 0, 4) {
          for (ic.inner, 0, 8) {
            unrolled (ow.c.inner, 0, 30) {
              vectorized (oc_block.c, 0, 8) {
                conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/8) + ic.outer)*16) + (c.outer.h.outer.fused % 16))*8) + (ic.inner % 8))*30) + ow.c.inner)])*int64(kernel_vec[(((((((c.outer.h.outer.fused/16)*4) + ((ic.inner/8) + ic.outer))*8) + (ic.inner % 8))*8) + oc_block.c)])))
              }
            }
          }
        }
      }
      for (ow, 0, 30) {
        vectorized (oc_block, 0, 8) {
          conv[((ow*8) + oc_block)] = conv.global[((ow*8) + oc_block)]
        }
      }
    }
    for (w.inner, 0, 30) {
      vectorized (c.inner, 0, 8) {
        output_unpack[(((((c.inner + ((c.outer.h.outer.fused/16)*8))*16) + (c.outer.h.outer.fused % 16))*30) + w.inner)] = conv[(((((c.inner/8)*30) + w.inner)*8) + (c.inner % 8))]
      }
    }
  }
}
