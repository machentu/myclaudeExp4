// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 8 * 34 * 8 * 62]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 8) {
      for (w, 0, 62) {
        data_vec[(((C.h.fused*496) + (c*62)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((C.h.fused/34)*15360) + (c*1920)) + ((C.h.fused % 34)*60)) + w) - 61)], (int8)0)
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
            kernel_vec[(((((((CO.h.fused/3)*4608) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)) + co)] = placeholder[(((((((CO.h.fused/3)*4608) + (co*576)) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w)]
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
                      conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + ow.c.inner) + kw)])*int64(kernel_vec[(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)) + oc_block.c)])))
                    }
                  }
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          vectorized (oc_block, 0, 8) {
            conv[(((ow.outer*240) + (ow.inner*8)) + oc_block)] = conv.global[((ow.inner*8) + oc_block)]
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        vectorized (c.inner, 0, 8) {
          output_unpack[((((((c.outer.h.fused/32)*15360) + (c.inner*1920)) + ((c.outer.h.fused % 32)*60)) + (w.outer*30)) + w.inner)] = conv[(((w.outer*240) + (w.inner*8)) + c.inner)]
        }
      }
    }
  }
}
