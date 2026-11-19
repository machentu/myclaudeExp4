// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 64 * 18 * 2 * 32]
produce data_vec {
  parallel (C.h.fused, 0, 1152) {
    for (c, 0, 2) {
      for (w, 0, 32) {
        data_vec[((((C.h.fused*2) + c)*32) + w)] = tvm_if_then_else((((((C.h.fused % 18) >= 1) && ((C.h.fused % 18) < 17)) && (w >= 1)) && (w < 31)), placeholder[((((((((C.h.fused/18)*2) + c)*16) + (C.h.fused % 18))*30) + w) + -31)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 64 * 3 * 3 * 2 * 3]
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 64) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          vectorized (co, 0, 3) {
            kernel_vec[(((((((((((CO.h.fused/3)*64) + CI)*3) + (CO.h.fused % 3))*3) + w)*2) + ci)*3) + co)] = placeholder[(((((((((CO.h.fused/3)*3) + co)*128) + ((CI*2) + ci))*3) + (CO.h.fused % 3))*3) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 272) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 30 * 3]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 30 * 3]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 30) {
          vectorized (oc_block.c.init, 0, 3) {
            conv.global[((ow.c.inner.init*3) + oc_block.c.init)] = (int64)0
          }
        }
        for (ic.outer, 0, 64) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 2) {
                unrolled (ow.c.inner, 0, 30) {
                  vectorized (oc_block.c, 0, 3) {
                    conv.global[((ow.c.inner*3) + oc_block.c)] = (conv.global[((ow.c.inner*3) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/2) + ic.outer)*18) + ((c.outer.h.fused % 16) + kh))*2) + (ic.inner % 2))*32) + (ow.c.inner + kw))])*int64(kernel_vec[(((((((((((c.outer.h.fused/16)*64) + ((ic.inner/2) + ic.outer))*3) + kh)*3) + kw)*2) + (ic.inner % 2))*3) + oc_block.c)])))
                  }
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 30) {
        vectorized (oc_block, 0, 3) {
          conv[((ow.inner*3) + oc_block)] = conv.global[((ow.inner*3) + oc_block)]
        }
      }
    }
    for (w.inner, 0, 30) {
      vectorized (c.inner, 0, 3) {
        output_unpack[(((((c.inner + ((c.outer.h.fused/16)*3))*16) + (c.outer.h.fused % 16))*30) + w.inner)] = conv[(((((c.inner/3)*30) + w.inner)*3) + (c.inner % 3))]
      }
    }
  }
}
