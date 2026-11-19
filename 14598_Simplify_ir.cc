// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 131072]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 8192]
produce data_vec {
  parallel (C.h.fused, 0, 1024) {
    for (c, 0, 8) {
      for (w, 0, 16) {
        data_vec[((((C.h.fused*8) + c)*16) + w)] = placeholder[(((((((C.h.fused/32)*8) + c)*32) + (C.h.fused % 32))*16) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 32) {
    for (CI, 0, 32) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*32) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*256) + CI)*8) + ci), 256, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 1024) {
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
        for (ic.outer, 0, 32) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 5)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 6)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 7)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 8)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 9)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 10)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 11)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 12)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 13)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 14)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*16) + 15)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
          }
        }
      }
    }
    for (w.inner, 0, 16) {
      output_unpack[ramp((((((c.outer.h.outer.fused/32)*256) + (c.outer.h.outer.fused % 32))*16) + w.inner), 512, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
