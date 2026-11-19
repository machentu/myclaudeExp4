// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 128]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 128]
// attr [conv.global] storage_scope = "global"
allocate conv.global[int64x8 * 1]
produce data_vec {
  parallel (C.h.fused, 0, 16) {
    for (c, 0, 8) {
      data_vec[((C.h.fused*8) + c)] = placeholder[((C.h.fused*8) + c)]
    }
  }
}
produce kernel_vec {
  for (CI, 0, 16) {
    for (ci, 0, 8) {
      kernel_vec[ramp((((CI*8) + ci)*8), 1, 8)] = placeholder[ramp(((CI*8) + ci), 128, 8)]
    }
  }
}
produce output_unpack {
  produce conv {
    produce conv.global {
      conv.global[ramp(0, 1, 8)] = x8((int64)0)
      for (ic.outer, 0, 16) {
        for (ic.inner, 0, 8) {
          conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[((ic.outer*8) + ic.inner)]))*int64x8(kernel_vec[ramp((((ic.outer*8) + ic.inner)*8), 1, 8)])))
        }
      }
    }
  }
  output_unpack[ramp(0, 1, 8)] = conv.global[ramp(0, 1, 8)]
}
