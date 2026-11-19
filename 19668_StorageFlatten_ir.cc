// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 8 * 34 * 8 * 62]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 8) {
      for (w, 0, 62) {
        data_vec[((((C.h.fused*8) + c)*62) + w)] = tvm_if_then_else((((((C.h.fused % 34) >= 1) && ((C.h.fused % 34) < 33)) && (w >= 1)) && (w < 61)), placeholder[((((((((C.h.fused/34)*8) + c)*32) + (C.h.fused % 34))*60) + w) + -61)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 8 * 8 * 3 * 3 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 24) {
    for (CI, 0, 8) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          vectorized (co, 0, 8) {
            kernel_vec[(((((((((((CO.h.fused/3)*8) + CI)*3) + (CO.h.fused % 3))*3) + w)*8) + ci)*8) + co)] = placeholder[(((((((((CO.h.fused/3)*8) + co)*64) + ((CI*8) + ci))*3) + (CO.h.fused % 3))*3) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 8]
    produce conv {
      for (ow.outer, 0, 2) {
        // attr [conv.global] storage_scope = "global"
        allocate conv.global[int64 * 1 * 1 * 1 * 30 * 8]
        produce conv.global {
          unrolled (ow.c.inner.init, 0, 30) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[((ow.c.inner.init*8) + oc_block.c.init)] = (int64)0
            }
          }
          for (ic.outer, 0, 8) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 8) {
                  unrolled (ow.c.inner, 0, 30) {
                    vectorized (oc_block.c, 0, 8) {
                      conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/8) + ic.outer)*34) + ((c.outer.h.fused % 32) + kh))*8) + (ic.inner % 8))*62) + ((ow.c.inner + (ow.outer*30)) + kw))])*int64(kernel_vec[(((((((((((c.outer.h.fused/32)*8) + ((ic.inner/8) + ic.outer))*3) + kh)*3) + kw)*8) + (ic.inner % 8))*8) + oc_block.c)])))
                    }
                  }
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          vectorized (oc_block, 0, 8) {
            conv[(((ow.inner + (ow.outer*30))*8) + oc_block)] = conv.global[((ow.inner*8) + oc_block)]
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        vectorized (c.inner, 0, 8) {
          output_unpack[(((((c.inner + ((c.outer.h.fused/32)*8))*32) + (c.outer.h.fused % 32))*60) + (w.inner + (w.outer*30)))] = conv[(((((c.inner/8)*60) + (w.inner + (w.outer*30)))*8) + (c.inner % 8))]
        }
      }
    }
  }
}
