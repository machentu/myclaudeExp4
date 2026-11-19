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
      kernel_vec[ramp(((CI*36) + (ci*6)), 1, 6)] = placeholder[ramp(((CI*6) + ci), 96, 6)]
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
      conv.global[ramp(0, 1, 6)] = x6((int64)0)
      for (ic.outer, 0, 16) {
        for (ic.inner, 0, 6) {
          conv.global[ramp(0, 1, 6)] = (conv.global[ramp(0, 1, 6)] + (x6(int64(data_vec[((ic.outer*6) + ic.inner)]))*int64x6(kernel_vec[ramp(((ic.outer*36) + (ic.inner*6)), 1, 6)])))
        }
      }
    }
    conv[ramp(0, 1, 6)] = conv.global[ramp(0, 1, 6)]
  }
  output_unpack[ramp(0, 1, 6)] = conv[ramp(0, 1, 6)]
}
