// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 17 * 10 * 3 * 17]
produce data_vec {
  parallel (C.h.fused, 0, 170) {
    for (c, 0, 3) {
      for (w, 0, 17) {
        data_vec[((((C.h.fused*3) + c)*17) + w)] = tvm_if_then_else((((((C.h.fused % 10) >= 1) && ((C.h.fused % 10) < 9)) && (w >= 1)) && (w < 16)), placeholder[((((((((C.h.fused/10)*3) + c)*8) + (C.h.fused % 10))*15) + w) + -16)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 17 * 3 * 3 * 3 * 3]
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 17) {
      for (w, 0, 3) {
        for (ci, 0, 3) {
          vectorized (co, 0, 3) {
            kernel_vec[(((((((((((CO.h.fused/3)*17) + CI)*3) + (CO.h.fused % 3))*3) + w)*3) + ci)*3) + co)] = placeholder[(((((((((CO.h.fused/3)*3) + co)*51) + ((CI*3) + ci))*3) + (CO.h.fused % 3))*3) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 136) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 15 * 3]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 15 * 3]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 15) {
          vectorized (oc_block.c.init, 0, 3) {
            conv.global[((ow.c.inner.init*3) + oc_block.c.init)] = (int64)0
          }
        }
        for (ic.outer, 0, 17) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 3) {
                unrolled (ow.c.inner, 0, 15) {
                  vectorized (oc_block.c, 0, 3) {
                    conv.global[((ow.c.inner*3) + oc_block.c)] = (conv.global[((ow.c.inner*3) + oc_block.c)] + (int64(data_vec[((((((((ic.inner/3) + ic.outer)*10) + ((c.outer.h.fused % 8) + kh))*3) + (ic.inner % 3))*17) + (ow.c.inner + kw))])*int64(kernel_vec[(((((((((((c.outer.h.fused/8)*17) + ((ic.inner/3) + ic.outer))*3) + kh)*3) + kw)*3) + (ic.inner % 3))*3) + oc_block.c)])))
                  }
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 15) {
        vectorized (oc_block, 0, 3) {
          conv[((ow.inner*3) + oc_block)] = conv.global[((ow.inner*3) + oc_block)]
        }
      }
    }
    for (w.inner, 0, 15) {
      vectorized (c.inner, 0, 3) {
        output_unpack[(((((c.inner + ((c.outer.h.fused/8)*3))*8) + (c.outer.h.fused % 8))*15) + w.inner)] = conv[(((((c.inner/3)*15) + w.inner)*3) + (c.inner % 3))]
      }
    }
  }
}
