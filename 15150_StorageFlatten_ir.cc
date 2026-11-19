// attr [data_vec] storage_scope = "global"
allocate data_vec[uint8 * 1 * 1 * 133 * 3 * 69]
produce data_vec {
  parallel (C.h.fused, 0, 133) {
    for (c, 0, 3) {
      for (w, 0, 69) {
        data_vec[((((C.h.fused*3) + c)*69) + w)] = tvm_if_then_else((((((C.h.fused % 133) >= 3) && ((C.h.fused % 133) < 131)) && (w >= 3)) && (w < 67)), placeholder[((((((((C.h.fused/133)*3) + c)*128) + (C.h.fused % 133))*64) + w) + -195)], (uint8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 8 * 1 * 7 * 7 * 3 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 56) {
    for (w, 0, 7) {
      for (ci, 0, 3) {
        vectorized (co, 0, 8) {
          kernel_vec[((((((CO.h.fused*7) + w)*3) + ci)*8) + co)] = placeholder[(((((((((CO.h.fused/7)*8) + co)*3) + ci)*7) + (CO.h.fused % 7))*7) + w)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 512) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 32 * 8]
    produce conv {
      for (ow.outer, 0, 2) {
        // attr [conv.global] storage_scope = "global"
        allocate conv.global[int64 * 1 * 1 * 1 * 16 * 8]
        produce conv.global {
          unrolled (ow.c.inner.init, 0, 16) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[((ow.c.inner.init*8) + oc_block.c.init)] = (int64)0
            }
          }
          for (kh, 0, 7) {
            for (kw, 0, 7) {
              for (ic.inner, 0, 3) {
                unrolled (ow.c.inner, 0, 16) {
                  vectorized (oc_block.c, 0, 8) {
                    conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[(((((((ic.inner/3)*133) + (((c.outer.h.fused % 64)*2) + kh))*3) + (ic.inner % 3))*69) + (((ow.c.inner + (ow.outer*16))*2) + kw))])*int64(kernel_vec[((((((((((c.outer.h.fused/64) + (ic.inner/3))*7) + kh)*7) + kw)*3) + (ic.inner % 3))*8) + oc_block.c)])))
                  }
                }
              }
            }
          }
        }
        for (ow.inner, 0, 16) {
          vectorized (oc_block, 0, 8) {
            conv[(((ow.inner + (ow.outer*16))*8) + oc_block)] = conv.global[((ow.inner*8) + oc_block)]
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 16) {
        vectorized (c.inner, 0, 8) {
          output_unpack[(((((c.inner + ((c.outer.h.fused/64)*8))*64) + (c.outer.h.fused % 64))*32) + (w.inner + (w.outer*16)))] = conv[(((((c.inner/8)*32) + (w.inner + (w.outer*16)))*8) + (c.inner % 8))]
        }
      }
    }
  }
}
