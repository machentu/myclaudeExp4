// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 1 * 1 * 6 * 1]
produce data_vec {
  for (c, 0, 6) {
    data_vec[c] = placeholder[c]
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 12 * 1 * 1 * 1 * 6 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 12) {
    for (ci, 0, 6) {
      vectorized (co, 0, 8) {
        kernel_vec[((((CO.h.fused*6) + ci)*8) + co)] = placeholder[((((CO.h.fused*8) + co)*6) + ci)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 12) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 1 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 1 * 8]
      produce conv.global {
        vectorized (oc_block.c.init, 0, 8) {
          conv.global[oc_block.c.init] = (int64)0
        }
        for (ic.inner, 0, 6) {
          vectorized (oc_block.c, 0, 8) {
            conv.global[oc_block.c] = (conv.global[oc_block.c] + (int64(data_vec[ic.inner])*int64(kernel_vec[(((ic.inner + (c.outer.h.outer.fused*6))*8) + oc_block.c)])))
          }
        }
      }
      vectorized (oc_block, 0, 8) {
        conv[oc_block] = conv.global[oc_block]
      }
    }
    vectorized (c.inner, 0, 8) {
      output_unpack[(c.inner + (c.outer.h.outer.fused*8))] = conv[c.inner]
    }
  }
}
