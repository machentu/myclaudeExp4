// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 17 * 8 * 3 * 15]
produce data_vec {
  parallel (C.h.fused, 0, 136) {
    for (c, 0, 3) {
      for (w, 0, 15) {
        data_vec[(((C.h.fused*45) + (c*15)) + w)] = placeholder[(((((C.h.fused/8)*360) + (c*120)) + ((C.h.fused % 8)*15)) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 17 * 1 * 1 * 3 * 3]
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        vectorized (co, 0, 3) {
          kernel_vec[((((CO.h.fused*153) + (CI*9)) + (ci*3)) + co)] = placeholder[((((CO.h.fused*153) + (co*51)) + (CI*3)) + ci)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 68) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 2 * 15 * 3]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 2 * 15 * 3]
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 2) {
          unrolled (ow.c.inner.init, 0, 15) {
            vectorized (oc_block.c.init, 0, 3) {
              conv.global[(((oh.c.inner.init*45) + (ow.c.inner.init*3)) + oc_block.c.init)] = (int64)0
            }
          }
        }
        for (ic.outer, 0, 17) {
          for (ic.inner, 0, 3) {
            unrolled (oh.c.inner, 0, 2) {
              unrolled (ow.c.inner, 0, 15) {
                vectorized (oc_block.c, 0, 3) {
                  conv.global[(((oh.c.inner*45) + (ow.c.inner*3)) + oc_block.c)] = (conv.global[(((oh.c.inner*45) + (ow.c.inner*3)) + oc_block.c)] + (int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (oh.c.inner*45)) + (ic.inner*15)) + ow.c.inner)])*int64(kernel_vec[(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 15) {
          vectorized (oc_block, 0, 3) {
            conv[(((oh.inner*45) + (ow*3)) + oc_block)] = conv.global[(((oh.inner*45) + (ow*3)) + oc_block)]
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        vectorized (c.inner, 0, 3) {
          output_unpack[((((((c.outer.h.outer.fused/4)*360) + (c.inner*120)) + ((c.outer.h.outer.fused % 4)*30)) + (h.inner*15)) + w.inner)] = conv[(((h.inner*45) + (w.inner*3)) + c.inner)]
        }
      }
    }
  }
}
