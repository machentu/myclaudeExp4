// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 24 * 32 * 8 * 60]
produce data_vec {
  parallel (C.h.fused, 0, 768) {
    for (c, 0, 8) {
      for (w, 0, 60) {
        data_vec[((((C.h.fused*8) + c)*60) + w)] = placeholder[(((((((C.h.fused/32)*8) + c)*32) + (C.h.fused % 32))*60) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 8 * 24 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 8) {
    for (CI, 0, 24) {
      for (ci, 0, 8) {
        vectorized (co, 0, 8) {
          kernel_vec[((((((CO.h.fused*24) + CI)*8) + ci)*8) + co)] = placeholder[((((CO.h.fused*8) + co)*192) + ((CI*8) + ci))]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 60 * 8]
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          unrolled (ow.c.inner.init, 0, 30) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[(((ow.c.inner.init + (ow.c.outer*30))*8) + oc_block.c.init)] = (int64)0
            }
          }
          for (ic.outer, 0, 24) {
            for (ic.inner, 0, 8) {
              unrolled (ow.c.inner, 0, 30) {
                vectorized (oc_block.c, 0, 8) {
                  conv.global[(((ow.c.inner + (ow.c.outer*30))*8) + oc_block.c)] = (conv.global[(((ow.c.inner + (ow.c.outer*30))*8) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/8) + ic.outer)*32) + (c.outer.h.outer.fused % 32))*8) + (ic.inner % 8))*60) + (ow.c.inner + (ow.c.outer*30)))])*int64(kernel_vec[(((((((c.outer.h.outer.fused/32)*24) + ((ic.inner/8) + ic.outer))*8) + (ic.inner % 8))*8) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (ow, 0, 60) {
        vectorized (oc_block, 0, 8) {
          conv[((ow*8) + oc_block)] = conv.global[((ow*8) + oc_block)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        vectorized (c.inner, 0, 8) {
          output_unpack[(((((c.inner + ((c.outer.h.outer.fused/32)*8))*32) + (c.outer.h.outer.fused % 32))*60) + (w.inner + (w.outer*30)))] = conv[(((((c.inner/8)*60) + (w.inner + (w.outer*30)))*8) + (c.inner % 8))]
        }
      }
    }
  }
}
