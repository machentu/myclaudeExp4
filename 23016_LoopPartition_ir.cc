// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 8 * 64 * 8 * 120]
produce data_vec {
  parallel (C.h.fused, 0, 512) {
    for (c, 0, 8) {
      for (w, 0, 120) {
        data_vec[(((C.h.fused*960) + (c*120)) + w)] = placeholder[(((((C.h.fused/64)*61440) + (c*7680)) + ((C.h.fused % 64)*120)) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 4 * 8 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 4) {
    for (CI, 0, 8) {
      for (ci, 0, 8) {
        vectorized (co, 0, 8) {
          kernel_vec[((((CO.h.fused*512) + (CI*64)) + (ci*8)) + co)] = placeholder[((((CO.h.fused*512) + (co*64)) + (CI*8)) + ci)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 120 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 120 * 8]
      produce conv.global {
        for (ow.c.outer, 0, 4) {
          unrolled (ow.c.inner.init, 0, 30) {
            vectorized (oc_block.c.init, 0, 8) {
              conv.global[(((ow.c.outer*240) + (ow.c.inner.init*8)) + oc_block.c.init)] = (int64)0
            }
          }
          for (ic.outer, 0, 8) {
            for (ic.inner, 0, 8) {
              unrolled (ow.c.inner, 0, 30) {
                vectorized (oc_block.c, 0, 8) {
                  conv.global[(((ow.c.outer*240) + (ow.c.inner*8)) + oc_block.c)] = (conv.global[(((ow.c.outer*240) + (ow.c.inner*8)) + oc_block.c)] + (int64(data_vec[(((((ic.outer*61440) + ((c.outer.h.outer.fused % 64)*960)) + (ic.inner*120)) + (ow.c.outer*30)) + ow.c.inner)])*int64(kernel_vec[(((((c.outer.h.outer.fused/64)*512) + (ic.outer*64)) + (ic.inner*8)) + oc_block.c)])))
                }
              }
            }
          }
        }
      }
      for (ow, 0, 120) {
        vectorized (oc_block, 0, 8) {
          conv[((ow*8) + oc_block)] = conv.global[((ow*8) + oc_block)]
        }
      }
    }
    for (w.outer, 0, 4) {
      for (w.inner, 0, 30) {
        vectorized (c.inner, 0, 8) {
          output_unpack[((((((c.outer.h.outer.fused/64)*61440) + (c.inner*7680)) + ((c.outer.h.outer.fused % 64)*120)) + (w.outer*30)) + w.inner)] = conv[(((w.outer*240) + (w.inner*8)) + c.inner)]
        }
      }
    }
  }
}
