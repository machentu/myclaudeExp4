// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 6120]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 136) {
    for (c, 0, 3) {
      for (w, 0, 15) {
        data_vec[((((C.h.fused*3) + c)*15) + w)] = placeholder[(((((((C.h.fused/8)*3) + c)*8) + (C.h.fused % 8))*15) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((((CO.h.fused*17) + CI)*3) + ci)*3), 1, 3)] = placeholder[ramp(((((CO.h.fused*51) + CI)*3) + ci), 51, 3)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 68) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 30]
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
        conv.global[ramp(45, 1, 3)] = x3((int64)0)
        conv.global[ramp(48, 1, 3)] = x3((int64)0)
        conv.global[ramp(51, 1, 3)] = x3((int64)0)
        conv.global[ramp(54, 1, 3)] = x3((int64)0)
        conv.global[ramp(57, 1, 3)] = x3((int64)0)
        conv.global[ramp(60, 1, 3)] = x3((int64)0)
        conv.global[ramp(63, 1, 3)] = x3((int64)0)
        conv.global[ramp(66, 1, 3)] = x3((int64)0)
        conv.global[ramp(69, 1, 3)] = x3((int64)0)
        conv.global[ramp(72, 1, 3)] = x3((int64)0)
        conv.global[ramp(75, 1, 3)] = x3((int64)0)
        conv.global[ramp(78, 1, 3)] = x3((int64)0)
        conv.global[ramp(81, 1, 3)] = x3((int64)0)
        conv.global[ramp(84, 1, 3)] = x3((int64)0)
        conv.global[ramp(87, 1, 3)] = x3((int64)0)
        for (ic.outer, 0, 17) {
          for (ic.inner, 0, 3) {
            conv.global[ramp(0, 1, 3)] = (conv.global[ramp(0, 1, 3)] + (x3(int64(data_vec[(((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(3, 1, 3)] = (conv.global[ramp(3, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 1)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(6, 1, 3)] = (conv.global[ramp(6, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 2)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(9, 1, 3)] = (conv.global[ramp(9, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 3)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(12, 1, 3)] = (conv.global[ramp(12, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 4)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(15, 1, 3)] = (conv.global[ramp(15, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 5)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(18, 1, 3)] = (conv.global[ramp(18, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 6)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(21, 1, 3)] = (conv.global[ramp(21, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 7)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(24, 1, 3)] = (conv.global[ramp(24, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 8)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(27, 1, 3)] = (conv.global[ramp(27, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 9)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(30, 1, 3)] = (conv.global[ramp(30, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 10)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(33, 1, 3)] = (conv.global[ramp(33, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 11)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(36, 1, 3)] = (conv.global[ramp(36, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 12)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(39, 1, 3)] = (conv.global[ramp(39, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 13)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(42, 1, 3)] = (conv.global[ramp(42, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 14)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(45, 1, 3)] = (conv.global[ramp(45, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 45)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(48, 1, 3)] = (conv.global[ramp(48, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 46)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(51, 1, 3)] = (conv.global[ramp(51, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 47)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(54, 1, 3)] = (conv.global[ramp(54, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 48)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(57, 1, 3)] = (conv.global[ramp(57, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 49)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(60, 1, 3)] = (conv.global[ramp(60, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 50)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(63, 1, 3)] = (conv.global[ramp(63, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 51)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(66, 1, 3)] = (conv.global[ramp(66, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 52)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(69, 1, 3)] = (conv.global[ramp(69, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 53)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(72, 1, 3)] = (conv.global[ramp(72, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 54)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(75, 1, 3)] = (conv.global[ramp(75, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 55)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(78, 1, 3)] = (conv.global[ramp(78, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 56)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(81, 1, 3)] = (conv.global[ramp(81, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 57)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(84, 1, 3)] = (conv.global[ramp(84, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 58)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            conv.global[ramp(87, 1, 3)] = (conv.global[ramp(87, 1, 3)] + (x3(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*6) + ic.inner)*15) + 59)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/4)*12) + (c.outer.h.outer.fused % 4))*2) + h.inner)*15) + w.inner), 120, 3)] = conv.global[ramp((((h.inner*15) + w.inner)*3), 1, 3)]
      }
    }
  }
}
