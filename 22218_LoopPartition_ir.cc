// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 32 * 10 * 8 * 17]
produce data_vec {
  parallel (C.h.fused, 0, 320) {
    for (c, 0, 8) {
      for (w, 0, 17) {
        data_vec[(((C.h.fused*136) + (c*17)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 10)) && ((C.h.fused % 10) < 9)) && (1 <= w)) && (w < 16)), placeholder[((((((C.h.fused/10)*960) + (c*120)) + ((C.h.fused % 10)*15)) + w) - 16)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 8 * 32 * 3 * 3 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 24) {
    for (CI, 0, 32) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          vectorized (co, 0, 8) {
            kernel_vec[(((((((CO.h.fused/3)*18432) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)) + co)] = placeholder[(((((((CO.h.fused/3)*18432) + (co*2304)) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 64) {
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
        for (ic.outer, 0, 32) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                unrolled (ow.c.inner, 0, 15) {
                  vectorized (oc_block.c, 0, 8) {
                    conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[((((((ic.outer*1360) + (kh*136)) + ((c.outer.h.fused % 8)*136)) + (ic.inner*17)) + ow.c.inner) + kw)])*int64(kernel_vec[(((((((c.outer.h.fused/8)*18432) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)) + oc_block.c)])))
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
        output_unpack[(((((c.outer.h.fused/8)*960) + (c.inner*120)) + ((c.outer.h.fused % 8)*15)) + w.inner)] = conv[((w.inner*8) + c.inner)]
      }
    }
  }
}
