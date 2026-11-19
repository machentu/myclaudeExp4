// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 16 * 1 * 4 * 1]
produce data_vec {
  parallel (C.h.fused, 0, 16) {
    for (c, 0, 4) {
      data_vec[((C.h.fused*4) + c)] = placeholder[((C.h.fused*4) + c)]
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 1 * 16 * 1 * 1 * 4 * 4]
produce kernel_vec {
  for (CI, 0, 16) {
    for (ci, 0, 4) {
      kernel_vec[ramp(((CI*16) + (ci*4)), 1, 4)] = placeholder[ramp(((CI*4) + ci), 64, 4)]
    }
  }
}
produce output_unpack {
  // attr [conv] storage_scope = "global"
  allocate conv[int64 * 1 * 1 * 1 * 1 * 4]
  produce conv {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 1 * 1 * 1 * 1 * 4]
    produce conv.global {
      conv.global[ramp(0, 1, 4)] = x4((int64)0)
      for (ic.outer, 0, 16) {
        for (ic.inner, 0, 4) {
          conv.global[ramp(0, 1, 4)] = (conv.global[ramp(0, 1, 4)] + (x4(int64(data_vec[((ic.outer*4) + ic.inner)]))*int64x4(kernel_vec[ramp(((ic.outer*16) + (ic.inner*4)), 1, 4)])))
        }
      }
    }
    conv[ramp(0, 1, 4)] = conv.global[ramp(0, 1, 4)]
  }
  output_unpack[ramp(0, 1, 4)] = conv[ramp(0, 1, 4)]
}
