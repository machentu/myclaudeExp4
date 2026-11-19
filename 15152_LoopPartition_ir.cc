// attr [data_vec] storage_scope = "global"
allocate data_vec[uint8 * 1 * 1 * 133 * 3 * 69]
produce data_vec {
  parallel (C.h.fused, 0, 133) {
    for (c, 0, 3) {
      for (w, 0, 69) {
        data_vec[(((C.h.fused*207) + (c*69)) + w)] = tvm_if_then_else(((((3 <= C.h.fused) && (C.h.fused < 131)) && (3 <= w)) && (w < 67)), placeholder[((((c*8192) + (C.h.fused*64)) + w) - 195)], (uint8)0)
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
          kernel_vec[((((CO.h.fused*168) + (w*24)) + (ci*8)) + co)] = placeholder[((((((CO.h.fused/7)*1176) + (co*147)) + (ci*49)) + ((CO.h.fused % 7)*7)) + w)]
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
                    conv.global[((ow.c.inner*8) + oc_block.c)] = (conv.global[((ow.c.inner*8) + oc_block.c)] + (int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (ow.c.inner*2)) + kw)])*int64(kernel_vec[((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)) + oc_block.c)])))
                  }
                }
              }
            }
          }
        }
        for (ow.inner, 0, 16) {
          vectorized (oc_block, 0, 8) {
            conv[(((ow.outer*128) + (ow.inner*8)) + oc_block)] = conv.global[((ow.inner*8) + oc_block)]
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 16) {
        vectorized (c.inner, 0, 8) {
          output_unpack[((((((c.outer.h.fused/64)*16384) + (c.inner*2048)) + ((c.outer.h.fused % 64)*32)) + (w.outer*16)) + w.inner)] = conv[(((w.outer*128) + (w.inner*8)) + c.inner)]
        }
      }
    }
  }
}
