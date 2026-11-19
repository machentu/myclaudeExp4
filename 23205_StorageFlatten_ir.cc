// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 17 * 32 * 3 * 60]
produce data_vec {
  parallel (C.h.fused, 0, 544) {
    for (c, 0, 3) {
      for (w, 0, 60) {
        data_vec[((((C.h.fused*3) + c)*60) + w)] = placeholder[(((((((C.h.fused/32)*3) + c)*32) + (C.h.fused % 32))*60) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 17 * 1 * 1 * 3 * 3]
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        vectorized (co, 0, 3) {
          kernel_vec[((((((CO.h.fused*17) + CI)*3) + ci)*3) + co)] = placeholder[((((CO.h.fused*3) + co)*51) + ((CI*3) + ci))]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 544) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 3]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 60 * 3]
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          unrolled (ow.c.inner.init, 0, 30) {
            vectorized (oc_block.c.init, 0, 3) {
              conv.global[(((ow.c.inner.init + (ow.c.outer*30))*3) + oc_block.c.init)] = (int64)0
            }
          }
          for (ic.outer, 0, 17) {
            for (ic.inner, 0, 3) {
              unrolled (ow.c.inner, 0, 30) {
                vectorized (oc_block.c, 0, 3) {
                  conv.global[(((ow.c.inner + (ow.c.outer*30))*3) + oc_block.c)] = (conv.global[(((ow.c.inner + (ow.c.outer*30))*3) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/3) + ic.outer)*32) + (c.outer.h.outer.fused % 32))*3) + (ic.inner % 3))*60) + (ow.c.inner + (ow.c.outer*30)))])*int64(kernel_vec[(((((((c.outer.h.outer.fused/32)*17) + ((ic.inner/3) + ic.outer))*3) + (ic.inner % 3))*3) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (ow, 0, 60) {
        vectorized (oc_block, 0, 3) {
          conv[((ow*3) + oc_block)] = conv.global[((ow*3) + oc_block)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        vectorized (c.inner, 0, 3) {
          output_unpack[(((((c.inner + ((c.outer.h.outer.fused/32)*3))*32) + (c.outer.h.outer.fused % 32))*60) + (w.inner + (w.outer*30)))] = conv[(((((c.inner/3)*60) + (w.inner + (w.outer*30)))*3) + (c.inner % 3))]
        }
      }
    }
  }
}
