// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 1 * 1 * 4 * 1]
produce data_vec {
  for (c, 0, 4) {
    data_vec[c] = placeholder[c]
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 8 * 1 * 1 * 1 * 4 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 8) {
    for (ci, 0, 4) {
      kernel_vec[ramp(((CO.h.fused*32) + (ci*8)), 1, 8)] = placeholder[ramp(((CO.h.fused*32) + ci), 4, 8)]
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 8) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 1 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 1 * 8]
      produce conv.global {
        conv.global[ramp(0, 1, 8)] = x8((int64)0)
        for (ic.inner, 0, 4) {
          conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[ic.inner]))*int64x8(kernel_vec[ramp(((c.outer.h.outer.fused*32) + (ic.inner*8)), 1, 8)])))
        }
      }
      conv[ramp(0, 1, 8)] = conv.global[ramp(0, 1, 8)]
    }
    output_unpack[ramp((c.outer.h.outer.fused*8), 1, 8)] = conv[ramp(0, 1, 8)]
  }
}
