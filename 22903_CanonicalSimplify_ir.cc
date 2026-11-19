// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 4 * 8 * 8 * 15]
produce data_vec {
  parallel (C.h.fused, 0, 32) {
    for (c, 0, 8) {
      for (w, 0, 15) {
        data_vec[(((C.h.fused*120) + (c*15)) + w)] = placeholder[(((((C.h.fused/8)*960) + (c*120)) + ((C.h.fused % 8)*15)) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 4 * 4 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 4) {
    for (CI, 0, 4) {
      for (ci, 0, 8) {
        vectorized (co, 0, 8) {
          kernel_vec[((((CO.h.fused*256) + (CI*64)) + (ci*8)) + co)] = placeholder[((((CO.h.fused*256) + (co*32)) + (CI*8)) + ci)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 16) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 2 * 15 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 2 * 15 * 8]
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 2) {
          unrolled (ow.c.inner.init, 0, 15) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[(((oh.c.inner.init*120) + (ow.c.inner.init*8)) + oc_block.c.init)] = (int64)0
            }
          }
        }
        for (ic.outer, 0, 4) {
          for (ic.inner, 0, 8) {
            unrolled (oh.c.inner, 0, 2) {
              unrolled (ow.c.inner, 0, 15) {
                vectorized (oc_block.c, 0, 8) {
                  conv.global[(((oh.c.inner*120) + (ow.c.inner*8)) + oc_block.c)] = (conv.global[(((oh.c.inner*120) + (ow.c.inner*8)) + oc_block.c)] + (int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (oh.c.inner*120)) + (ic.inner*15)) + ow.c.inner)])*int64(kernel_vec[(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 15) {
          vectorized (oc_block, 0, 8) {
            conv[(((oh.inner*120) + (ow*8)) + oc_block)] = conv.global[(((oh.inner*120) + (ow*8)) + oc_block)]
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        vectorized (c.inner, 0, 8) {
          output_unpack[((((((c.outer.h.outer.fused/4)*960) + (c.inner*120)) + ((c.outer.h.outer.fused % 4)*30)) + (h.inner*15)) + w.inner)] = conv[(((h.inner*120) + (w.inner*8)) + c.inner)]
        }
      }
    }
  }
}
