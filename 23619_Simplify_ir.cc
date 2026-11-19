// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 43520]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 39168]
produce data_vec {
  parallel (C.h.fused, 0, 1280) {
    for (c, 0, 2) {
      for (w, 0, 17) {
        data_vec[((((C.h.fused*2) + c)*17) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 10)) && ((C.h.fused % 10) < 9)) && (1 <= w)) && (w < 16)), placeholder[((((((((C.h.fused/10)*2) + c)*8) + (C.h.fused % 10))*15) + w) + -16)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 128) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          kernel_vec[ramp(((((((((((CO.h.fused/3)*128) + CI)*3) + (CO.h.fused % 3))*3) + w)*2) + ci)*3), 1, 3)] = placeholder[ramp((((((((((CO.h.fused/3)*384) + CI)*2) + ci)*3) + (CO.h.fused % 3))*3) + w), 2304, 3)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 136) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 15]
    produce conv {
      produce conv.global {
        conv.global[ramp(0, 1, 3)] = x3((int64)0)
        conv.global[ramp(3, 1, 3)] = x3((int64)0)
        conv.global[ramp(6, 1, 3)] = x3((int64)0)
        conv.global[ramp(9, 1, 3)] = x3((int64)0)
        conv.global[ramp(12, 1, 3)] = x3((int64)0)
        conv.global[ramp(15, 1, 3)] = x3((int64)0)
        conv.global[ramp(18, 1, 3)] = x3((int64)0)
        conv.global[ramp(21, 1, 3)] = x3((int64)0)
        conv.global[ramp(24, 1, 3)] = x3((int64)0)
        conv.global[ramp(27, 1, 3)] = x3((int64)0)
        conv.global[ramp(30, 1, 3)] = x3((int64)0)
        conv.global[ramp(33, 1, 3)] = x3((int64)0)
        conv.global[ramp(36, 1, 3)] = x3((int64)0)
        conv.global[ramp(39, 1, 3)] = x3((int64)0)
        conv.global[ramp(42, 1, 3)] = x3((int64)0)
        for (ic.outer, 0, 128) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 2) {
                conv.global[ramp(0, 1, 3)] = (conv.global[ramp(0, 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(3, 1, 3)] = (conv.global[ramp(3, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 1)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(6, 1, 3)] = (conv.global[ramp(6, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 2)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(9, 1, 3)] = (conv.global[ramp(9, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 3)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(12, 1, 3)] = (conv.global[ramp(12, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 4)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(15, 1, 3)] = (conv.global[ramp(15, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 5)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(18, 1, 3)] = (conv.global[ramp(18, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 6)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(21, 1, 3)] = (conv.global[ramp(21, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 7)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(24, 1, 3)] = (conv.global[ramp(24, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 8)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(27, 1, 3)] = (conv.global[ramp(27, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 9)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(30, 1, 3)] = (conv.global[ramp(30, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 10)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(33, 1, 3)] = (conv.global[ramp(33, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 11)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(36, 1, 3)] = (conv.global[ramp(36, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 12)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(39, 1, 3)] = (conv.global[ramp(39, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 13)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                conv.global[ramp(42, 1, 3)] = (conv.global[ramp(42, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*10) + kh) + (c.outer.h.fused % 8))*2) + ic.inner)*17) + kw) + 14)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/8)*128) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
              }
            }
          }
        }
      }
    }
    for (w.inner, 0, 15) {
      output_unpack[ramp((((((c.outer.h.fused/8)*24) + (c.outer.h.fused % 8))*15) + w.inner), 120, 3)] = conv.global[ramp((w.inner*3), 1, 3)]
    }
  }
}
