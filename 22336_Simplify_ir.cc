// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 9600]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 800]
produce data_vec {
  parallel (C.h.fused, 0, 80) {
    for (c, 0, 8) {
      for (w, 0, 15) {
        data_vec[((((C.h.fused*8) + c)*15) + w)] = placeholder[(((((((C.h.fused/8)*8) + c)*8) + (C.h.fused % 8))*15) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 10) {
    for (CI, 0, 10) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*10) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*80) + CI)*8) + ci), 80, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 40) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
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
        conv.global[ramp(128, 1, 8)] = x8((int64)0)
        conv.global[ramp(136, 1, 8)] = x8((int64)0)
        conv.global[ramp(144, 1, 8)] = x8((int64)0)
        conv.global[ramp(152, 1, 8)] = x8((int64)0)
        conv.global[ramp(160, 1, 8)] = x8((int64)0)
        conv.global[ramp(168, 1, 8)] = x8((int64)0)
        conv.global[ramp(176, 1, 8)] = x8((int64)0)
        conv.global[ramp(184, 1, 8)] = x8((int64)0)
        conv.global[ramp(192, 1, 8)] = x8((int64)0)
        conv.global[ramp(200, 1, 8)] = x8((int64)0)
        conv.global[ramp(208, 1, 8)] = x8((int64)0)
        conv.global[ramp(216, 1, 8)] = x8((int64)0)
        conv.global[ramp(224, 1, 8)] = x8((int64)0)
        conv.global[ramp(232, 1, 8)] = x8((int64)0)
        for (ic.outer, 0, 10) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 5)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 6)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 7)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 8)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 9)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 10)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 11)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 12)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 13)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 14)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 120)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 121)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 122)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 123)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 124)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 125)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 126)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 127)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 128)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 129)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 130)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 131)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 132)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 133)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 134)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*10) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/4)*32) + (c.outer.h.outer.fused % 4))*2) + h.inner)*15) + w.inner), 120, 8)] = conv.global[ramp((((h.inner*15) + w.inner)*8), 1, 8)]
      }
    }
  }
}
