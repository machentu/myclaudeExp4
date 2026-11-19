// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 8]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 128]
produce data_vec {
  for (c, 0, 8) {
    data_vec[c] = placeholder[c]
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 16) {
    for (ci, 0, 8) {
      kernel_vec[ramp(((CO.h.fused*64) + (ci*8)), 1, 8)] = placeholder[ramp(((CO.h.fused*64) + ci), 8, 8)]
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 16) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 1]
    produce conv {
      produce conv.global {
        conv.global[ramp(0, 1, 8)] = x8((int64)0)
        for (ic.inner, 0, 8) {
          conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[ic.inner]))*int64x8(kernel_vec[ramp(((c.outer.h.outer.fused*64) + (ic.inner*8)), 1, 8)])))
        }
      }
      conv.global[ramp(0, 1, 8)] = conv.global[ramp(0, 1, 8)]
    }
    output_unpack[ramp((c.outer.h.outer.fused*8), 1, 8)] = conv.global[ramp(0, 1, 8)]
  }
}
