// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 16 * 17 * 8 * 31]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 8) {
      for (w, 0, 31) {
        data_vec[((((C.h.fused*8) + c)*31) + w)] = tvm_if_then_else((((((C.h.fused % 17) >= 1) && ((C.h.fused % 17) < 17)) && (w >= 1)) && (w < 31)), placeholder[((((((((C.h.fused/17)*8) + c)*16) + (C.h.fused % 17))*30) + w) + -31)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 32 * 16 * 3 * 3 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 96) {
    for (CI, 0, 16) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          vectorized (co, 0, 8) {
            kernel_vec[(((((((((((CO.h.fused/3)*16) + CI)*3) + (CO.h.fused % 3))*3) + w)*8) + ci)*8) + co)] = placeholder[(((((((((CO.h.fused/3)*8) + co)*128) + ((CI*8) + ci))*3) + (CO.h.fused % 3))*3) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 15 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 15 * 8]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 15) {
          vectorized (oc_block.c.init, 0, 8) {
            conv.global[((ow.c.inner.init*8) + oc_block.c.init)] = (int64)0
          }
        }
        for (ic.outer, 0, 16) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                unrolled (ow.c.inner, 0, 15) {
                  vectorized (oc_block.c, 0, 8) {
                    conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/8) + ic.outer)*17) + (((c.outer.h.fused % 8)*2) + kh))*8) + (ic.inner % 8))*31) + ((ow.c.inner*2) + kw))])*int64(kernel_vec[(((((((((((c.outer.h.fused/8)*16) + ((ic.inner/8) + ic.outer))*3) + kh)*3) + kw)*8) + (ic.inner % 8))*8) + oc_block.c)])))
                  }
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 15) {
        vectorized (oc_block, 0, 8) {
          conv[((ow.inner*8) + oc_block)] = conv.global[((ow.inner*8) + oc_block)]
        }
      }
    }
    for (w.inner, 0, 15) {
      vectorized (c.inner, 0, 8) {
        output_unpack[(((((c.inner + ((c.outer.h.fused/8)*8))*8) + (c.outer.h.fused % 8))*15) + w.inner)] = conv[(((((c.inner/8)*15) + w.inner)*8) + (c.inner % 8))]
      }
    }
  }
}
