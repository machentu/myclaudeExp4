// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 96]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x6 * 96]
// attr [conv.global] storage_scope = "global"
allocate conv.global[int64x6 * 1]
produce data_vec {
  parallel (C.h.fused, 0, 16) {
    for (c, 0, 6) {
      data_vec[((C.h.fused*6) + c)] = placeholder[((C.h.fused*6) + c)]
    }
  }
}
produce kernel_vec {
  for (CI, 0, 16) {
    for (ci, 0, 6) {
      kernel_vec[ramp((((CI*6) + ci)*6), 1, 6)] = placeholder[ramp(((CI*6) + ci), 96, 6)]
    }
  }
}
produce output_unpack {
  produce conv {
    produce conv.global {
      conv.global[ramp(0, 1, 6)] = x6((int64)0)
      for (ic.outer, 0, 16) {
        for (ic.inner, 0, 6) {
          conv.global[ramp(0, 1, 6)] = (conv.global[ramp(0, 1, 6)] + (x6(int64(data_vec[((ic.outer*6) + ic.inner)]))*int64x6(kernel_vec[ramp((((ic.outer*6) + ic.inner)*6), 1, 6)])))
        }
      }
    }
  }
  output_unpack[ramp(0, 1, 6)] = conv.global[ramp(0, 1, 6)]
}
