// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 184320]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 6144]
produce data_vec {
  parallel (C.h.fused, 0, 768) {
    for (c, 0, 8) {
      for (w, 0, 30) {
        data_vec[((((C.h.fused*8) + c)*30) + w)] = placeholder[(((((((C.h.fused/16)*8) + c)*16) + (C.h.fused % 16))*30) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 16) {
    for (CI, 0, 48) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*48) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*384) + CI)*8) + ci), 384, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
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
        for (ic.outer, 0, 48) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 5)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 6)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 7)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 8)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 9)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 10)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 11)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 12)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 13)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 14)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 15)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 16)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 17)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 18)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 19)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 20)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 21)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 22)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 23)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 24)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 25)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 26)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 27)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 28)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16) + (c.outer.h.outer.fused % 16))*8) + ic.inner)*30) + 29)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/16)*48) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
          }
        }
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp((((((c.outer.h.outer.fused/16)*128) + (c.outer.h.outer.fused % 16))*30) + w.inner), 480, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
