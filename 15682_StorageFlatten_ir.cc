// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 16 * 1 * 6 * 1]
produce data_vec {
  parallel (C.h.fused, 0, 16) {
    for (c, 0, 6) {
      data_vec[((C.h.fused*6) + c)] = placeholder[((C.h.fused*6) + c)]
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 1 * 16 * 1 * 1 * 6 * 6]
produce kernel_vec {
  for (CI, 0, 16) {
    for (ci, 0, 6) {
      vectorized (co, 0, 6) {
        kernel_vec[((((CI*6) + ci)*6) + co)] = placeholder[((co*96) + ((CI*6) + ci))]
      }
    }
  }
}
produce output_unpack {
  // attr [conv] storage_scope = "global"
  allocate conv[int64 * 1 * 1 * 1 * 1 * 6]
  produce conv {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 1 * 1 * 1 * 1 * 6]
    produce conv.global {
      vectorized (oc_block.c.init, 0, 6) {
        conv.global[oc_block.c.init] = (int64)0
      }
      for (ic.outer, 0, 16) {
        for (ic.inner, 0, 6) {
          vectorized (oc_block.c, 0, 6) {
            conv.global[oc_block.c] = (conv.global[oc_block.c] + (int64(data_vec[((((ic.inner/6) + ic.outer)*6) + (ic.inner % 6))])*int64(kernel_vec[((((((ic.inner/6) + ic.outer)*6) + (ic.inner % 6))*6) + oc_block.c)])))
          }
        }
      }
    }
    vectorized (oc_block, 0, 6) {
      conv[oc_block] = conv.global[oc_block]
    }
  }
  vectorized (c.inner, 0, 6) {
    output_unpack[c.inner] = conv[c.inner]
  }
}
