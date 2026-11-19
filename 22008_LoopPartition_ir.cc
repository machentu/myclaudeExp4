// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 16 * 18 * 8 * 32]
produce data_vec {
  parallel (C.h.fused, 0, 288) {
    for (c, 0, 8) {
      for (w, 0, 32) {
        data_vec[(((C.h.fused*256) + (c*32)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 18)) && ((C.h.fused % 18) < 17)) && (1 <= w)) && (w < 31)), placeholder[((((((C.h.fused/18)*3840) + (c*480)) + ((C.h.fused % 18)*30)) + w) - 31)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 10 * 16 * 3 * 3 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 30) {
    for (CI, 0, 16) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          vectorized (co, 0, 8) {
            kernel_vec[(((((((CO.h.fused/3)*9216) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)) + co)] = placeholder[(((((((CO.h.fused/3)*9216) + (co*1152)) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w)]
          }
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 160) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 30 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 30 * 8]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 30) {
          vectorized (oc_block.c.init, 0, 8) {
            conv.global[((ow.c.inner.init*8) + oc_block.c.init)] = (int64)0
          }
        }
        for (ic.outer, 0, 16) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                unrolled (ow.c.inner, 0, 30) {
                  vectorized (oc_block.c, 0, 8) {
                    conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[((((((ic.outer*4608) + (kh*256)) + ((c.outer.h.fused % 16)*256)) + (ic.inner*32)) + ow.c.inner) + kw)])*int64(kernel_vec[(((((((c.outer.h.fused/16)*9216) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)) + oc_block.c)])))
                  }
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 30) {
        vectorized (oc_block, 0, 8) {
          conv[((ow.inner*8) + oc_block)] = conv.global[((ow.inner*8) + oc_block)]
        }
      }
    }
    for (w.inner, 0, 30) {
      vectorized (c.inner, 0, 8) {
        output_unpack[(((((c.outer.h.fused/16)*3840) + (c.inner*480)) + ((c.outer.h.fused % 16)*30)) + w.inner)] = conv[((w.inner*8) + c.inner)]
      }
    }
  }
}
