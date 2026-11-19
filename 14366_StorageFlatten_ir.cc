// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 12 * 16 * 8 * 8]
produce data_vec {
  parallel (C.h.fused, 0, 192) {
    for (c, 0, 8) {
      for (w, 0, 8) {
        data_vec[((((C.h.fused*8) + c)*8) + w)] = placeholder[(((((((C.h.fused/16)*8) + c)*16) + (C.h.fused % 16))*8) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 12 * 12 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 12) {
    for (CI, 0, 12) {
      for (ci, 0, 8) {
        vectorized (co, 0, 8) {
          kernel_vec[((((((CO.h.fused*12) + CI)*8) + ci)*8) + co)] = placeholder[((((CO.h.fused*8) + co)*96) + ((CI*8) + ci))]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 96) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 2 * 8 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 2 * 8 * 8]
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 2) {
          unrolled (ow.c.inner.init, 0, 8) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[((((oh.c.inner.init*8) + ow.c.inner.init)*8) + oc_block.c.init)] = (int64)0
            }
          }
        }
        for (ic.outer, 0, 12) {
          for (ic.inner, 0, 8) {
            unrolled (oh.c.inner, 0, 2) {
              unrolled (ow.c.inner, 0, 8) {
                vectorized (oc_block.c, 0, 8) {
                  conv.global[((((oh.c.inner*8) + ow.c.inner)*8) + oc_block.c)] = (conv.global[((((oh.c.inner*8) + ow.c.inner)*8) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/8) + ic.outer)*16) + (oh.c.inner + ((c.outer.h.outer.fused % 8)*2)))*8) + (ic.inner % 8))*8) + ow.c.inner)])*int64(kernel_vec[(((((((c.outer.h.outer.fused/8)*12) + ((ic.inner/8) + ic.outer))*8) + (ic.inner % 8))*8) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 8) {
          vectorized (oc_block, 0, 8) {
            conv[((((oh.inner*8) + ow)*8) + oc_block)] = conv.global[((((oh.inner*8) + ow)*8) + oc_block)]
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 8) {
        vectorized (c.inner, 0, 8) {
          output_unpack[(((((c.inner + ((c.outer.h.outer.fused/8)*8))*16) + (h.inner + ((c.outer.h.outer.fused % 8)*2)))*8) + w.inner)] = conv[(((((((c.inner/8)*2) + h.inner)*8) + w.inner)*8) + (c.inner % 8))]
        }
      }
    }
  }
}
