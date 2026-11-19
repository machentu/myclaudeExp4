// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 32 * 34 * 2 * 62]
produce data_vec {
  parallel (C.h.fused, 0, 1088) {
    for (c, 0, 2) {
      for (w, 0, 62) {
        data_vec[(((C.h.fused*124) + (c*62)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((C.h.fused/34)*3840) + (c*1920)) + ((C.h.fused % 34)*60)) + w) - 61)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 32 * 3 * 3 * 2 * 3]
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 32) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          vectorized (co, 0, 3) {
            kernel_vec[(((((((CO.h.fused/3)*1728) + (CI*54)) + ((CO.h.fused % 3)*18)) + (w*6)) + (ci*3)) + co)] = placeholder[(((((((CO.h.fused/3)*1728) + (co*576)) + (CI*18)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 544) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 3]
    produce conv {
      for (ow.outer, 0, 2) {
        // attr [conv.global] storage_scope = "global"
        allocate conv.global[int64 * 1 * 1 * 1 * 30 * 3]
        produce conv.global {
          unrolled (ow.c.inner.init, 0, 30) {
            vectorized (oc_block.c.init, 0, 3) {
              conv.global[((ow.c.inner.init*3) + oc_block.c.init)] = (int64)0
            }
          }
          for (ic.outer, 0, 32) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 2) {
                  unrolled (ow.c.inner, 0, 30) {
                    vectorized (oc_block.c, 0, 3) {
                      conv.global[((ow.c.inner*3) + oc_block.c)] = (conv.global[((ow.c.inner*3) + oc_block.c)] + (int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + ow.c.inner) + kw)])*int64(kernel_vec[(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)) + oc_block.c)])))
                    }
                  }
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          vectorized (oc_block, 0, 3) {
            conv[(((ow.outer*90) + (ow.inner*3)) + oc_block)] = conv.global[((ow.inner*3) + oc_block)]
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        vectorized (c.inner, 0, 3) {
          output_unpack[((((((c.outer.h.fused/32)*5760) + (c.inner*1920)) + ((c.outer.h.fused % 32)*60)) + (w.outer*30)) + w.inner)] = conv[(((w.outer*90) + (w.inner*3)) + c.inner)]
        }
      }
    }
  }
}
