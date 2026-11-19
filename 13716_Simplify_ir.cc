// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 16384]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 8192]
produce data_vec {
  parallel (C.h.fused, 0, 512) {
    for (c, 0, 8) {
      for (w, 0, 4) {
        data_vec[((((C.h.fused*8) + c)*4) + w)] = placeholder[(((((((C.h.fused/8)*8) + c)*8) + (C.h.fused % 8))*4) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 16) {
    for (CI, 0, 64) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*64) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*512) + CI)*8) + ci), 512, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 32) {
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
        for (ic.outer, 0, 64) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 32)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 33)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 34)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 35)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 64)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 65)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 66)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 67)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 96)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 97)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 98)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*2) + (c.outer.h.outer.fused % 2))*32) + ic.inner)*4) + 99)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/2)*64) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
          }
        }
      }
    }
    for (h.inner, 0, 4) {
      for (w.inner, 0, 4) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/2)*16) + (c.outer.h.outer.fused % 2))*4) + h.inner)*4) + w.inner), 32, 8)] = conv.global[ramp((((h.inner*4) + w.inner)*8), 1, 8)]
      }
    }
  }
}
