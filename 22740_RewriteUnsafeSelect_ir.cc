// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 43520]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 18432]
produce data_vec {
  parallel (C.h.fused, 0, 320) {
    for (c, 0, 8) {
      for (w, 0, 17) {
        data_vec[((((C.h.fused*8) + c)*17) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 10)) && ((C.h.fused % 10) < 9)) && (1 <= w)) && (w < 16)), placeholder[((((((((C.h.fused/10)*8) + c)*8) + (C.h.fused % 10))*15) + w) + -16)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 24) {
    for (CI, 0, 32) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((((((CO.h.fused/3)*32) + CI)*3) + (CO.h.fused % 3))*3) + w)*8) + ci)*8), 1, 8)] = placeholder[ramp((((((((((CO.h.fused/3)*256) + CI)*8) + ci)*3) + (CO.h.fused % 3))*3) + w), 2304, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 64) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 15]
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
        for (ic.outer, 0, 32) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 1)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 2)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 3)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 4)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 5)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 6)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 7)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 8)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 9)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 10)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 11)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 12)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 13)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*8) + ic.inner)*17) + kw) + 14)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*32) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
              }
            }
          }
        }
      }
    }
    for (w.inner, 0, 15) {
      output_unpack[ramp((((((c.outer.h.fused/8)*64) + (c.outer.h.fused % 8))*15) + w.inner), 120, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
