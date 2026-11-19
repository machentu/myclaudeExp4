// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 16 * 1 * 8 * 1]
produce data_vec {
  parallel (C.h.fused, 0, 16) {
    for (c, 0, 8) {
      data_vec[((C.h.fused*8) + c)] = placeholder[((C.h.fused*8) + c)]
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 1 * 16 * 1 * 1 * 8 * 8]
produce kernel_vec {
  for (CI, 0, 16) {
    for (ci, 0, 8) {
      kernel_vec[ramp(((CI*64) + (ci*8)), 1, 8)] = placeholder[ramp(((CI*8) + ci), 128, 8)]
    }
  }
}
produce output_unpack {
  // attr [conv] storage_scope = "global"
  allocate conv[int64 * 1 * 1 * 1 * 1 * 8]
  produce conv {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 1 * 1 * 1 * 1 * 8]
    produce conv.global {
      conv.global[ramp(0, 1, 8)] = x8((int64)0)
      for (ic.outer, 0, 16) {
        for (ic.inner, 0, 8) {
          conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[((ic.outer*8) + ic.inner)]))*int64x8(kernel_vec[ramp(((ic.outer*64) + (ic.inner*8)), 1, 8)])))
        }
      }
    }
    conv[ramp(0, 1, 8)] = conv.global[ramp(0, 1, 8)]
  }
  output_unpack[ramp(0, 1, 8)] = conv[ramp(0, 1, 8)]
}
