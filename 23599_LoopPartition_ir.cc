// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 17 * 10 * 3 * 17]
produce data_vec {
  parallel (C.h.fused, 0, 170) {
    for (c, 0, 3) {
      for (w, 0, 17) {
        data_vec[(((C.h.fused*51) + (c*17)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 10)) && ((C.h.fused % 10) < 9)) && (1 <= w)) && (w < 16)), placeholder[((((((C.h.fused/10)*360) + (c*120)) + ((C.h.fused % 10)*15)) + w) - 16)], (int8)0)
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
            kernel_vec[(((((((CO.h.fused/3)*1377) + (CI*81)) + ((CO.h.fused % 3)*27)) + (w*9)) + (ci*3)) + co)] = placeholder[(((((((CO.h.fused/3)*1377) + (co*459)) + (CI*27)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w)]
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
                    conv.global[((ow.c.inner*3) + oc_block.c)] = (conv.global[((ow.c.inner*3) + oc_block.c)] + (int64(data_vec[((((((ic.outer*510) + (kh*51)) + ((c.outer.h.fused % 8)*51)) + (ic.inner*17)) + ow.c.inner) + kw)])*int64(kernel_vec[(((((((c.outer.h.fused/8)*1377) + (ic.outer*81)) + (kh*27)) + (kw*9)) + (ic.inner*3)) + oc_block.c)])))
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
        output_unpack[(((((c.outer.h.fused/8)*360) + (c.inner*120)) + ((c.outer.h.fused % 8)*15)) + w.inner)] = conv[((w.inner*3) + c.inner)]
      }
    }
  }
}
