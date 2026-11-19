// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 48 * 8 * 8 * 4]
produce data_vec {
  parallel (C.h.fused, 0, 384) {
    for (c, 0, 8) {
      for (w, 0, 4) {
        data_vec[(((C.h.fused*32) + (c*4)) + w)] = placeholder[(((((C.h.fused/8)*256) + (c*32)) + ((C.h.fused % 8)*4)) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 16 * 48 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 16) {
    for (CI, 0, 48) {
      for (ci, 0, 8) {
        vectorized (co, 0, 8) {
          kernel_vec[((((CO.h.fused*3072) + (CI*64)) + (ci*8)) + co)] = placeholder[((((CO.h.fused*3072) + (co*384)) + (CI*8)) + ci)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 32) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 4 * 4 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 4 * 4 * 8]
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 4) {
          unrolled (ow.c.inner.init, 0, 4) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[(((oh.c.inner.init*32) + (ow.c.inner.init*8)) + oc_block.c.init)] = (int64)0
            }
          }
        }
        for (ic.outer, 0, 48) {
          for (ic.inner, 0, 8) {
            unrolled (oh.c.inner, 0, 4) {
              unrolled (ow.c.inner, 0, 4) {
                vectorized (oc_block.c, 0, 8) {
                  conv.global[(((oh.c.inner*32) + (ow.c.inner*8)) + oc_block.c)] = (conv.global[(((oh.c.inner*32) + (ow.c.inner*8)) + oc_block.c)] + (int64(data_vec[(((((ic.outer*256) + ((c.outer.h.outer.fused % 2)*128)) + (oh.c.inner*32)) + (ic.inner*4)) + ow.c.inner)])*int64(kernel_vec[(((((c.outer.h.outer.fused/2)*3072) + (ic.outer*64)) + (ic.inner*8)) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (oh.inner, 0, 4) {
        for (ow, 0, 4) {
          vectorized (oc_block, 0, 8) {
            conv[(((oh.inner*32) + (ow*8)) + oc_block)] = conv.global[(((oh.inner*32) + (ow*8)) + oc_block)]
          }
        }
      }
    }
    for (h.inner, 0, 4) {
      for (w.inner, 0, 4) {
        vectorized (c.inner, 0, 8) {
          output_unpack[((((((c.outer.h.outer.fused/2)*256) + (c.inner*32)) + ((c.outer.h.outer.fused % 2)*16)) + (h.inner*4)) + w.inner)] = conv[(((h.inner*32) + (w.inner*8)) + c.inner)]
        }
      }
    }
  }
}
