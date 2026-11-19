// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 12288]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 4608]
produce data_vec {
  parallel (C.h.fused, 0, 192) {
    for (c, 0, 8) {
      for (w, 0, 8) {
        data_vec[((((C.h.fused*8) + c)*8) + w)] = placeholder[(((((((C.h.fused/16)*8) + c)*16) + (C.h.fused % 16))*8) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 48) {
    for (CI, 0, 12) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*12) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*96) + CI)*8) + ci), 96, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 384) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 16]
    produce conv {
      produce conv.global {
        conv.global[ramp(0, 1, 8)] = x8((int64)0)
        conv.global[ramp(8, 1, 8)] = x8((int64)0)
        conv.global[ramp(16, 1, 8)] = x8((int64)0)
        conv.global[ramp(24, 1, 8)] = x8((int64)0)
        conv.global[ramp(32, 1, 8)] = x8((int64)0)
        conv.global[ramp(40, 1, 8)] = x8((int64)0)
        conv.global[ramp(48, 1, 8)] = x8((int64)0)
        conv.global[ramp(56, 1, 8)] = x8((int64)0)
        conv.global[ramp(64, 1, 8)] = x8((int64)0)
        conv.global[ramp(72, 1, 8)] = x8((int64)0)
        conv.global[ramp(80, 1, 8)] = x8((int64)0)
        conv.global[ramp(88, 1, 8)] = x8((int64)0)
        conv.global[ramp(96, 1, 8)] = x8((int64)0)
        conv.global[ramp(104, 1, 8)] = x8((int64)0)
        conv.global[ramp(112, 1, 8)] = x8((int64)0)
        conv.global[ramp(120, 1, 8)] = x8((int64)0)
        for (ic.outer, 0, 12) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 5)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 6)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 7)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 64)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 65)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 66)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 67)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 68)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 69)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 70)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*8) + (c.outer.h.outer.fused % 8))*16) + ic.inner)*8) + 71)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/8)*12) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 8) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/8)*64) + (c.outer.h.outer.fused % 8))*2) + h.inner)*8) + w.inner), 128, 8)] = conv.global[ramp((((h.inner*8) + w.inner)*8), 1, 8)]
      }
    }
  }
}
