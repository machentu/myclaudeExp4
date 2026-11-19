// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 134912]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 9792]
produce data_vec {
  parallel (C.h.fused, 0, 1088) {
    for (c, 0, 2) {
      for (w, 0, 62) {
        data_vec[((((C.h.fused*2) + c)*62) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((((C.h.fused/34)*2) + c)*32) + (C.h.fused % 34))*60) + w) + -61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 32) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          kernel_vec[ramp(((((((((((CO.h.fused/3)*32) + CI)*3) + (CO.h.fused % 3))*3) + w)*2) + ci)*3), 1, 3)] = placeholder[ramp((((((((((CO.h.fused/3)*96) + CI)*2) + ci)*3) + (CO.h.fused % 3))*3) + w), 576, 3)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 544) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x3 * 60]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 30]
    produce conv {
      for (ow.outer, 0, 2) {
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
          for (ic.outer, 0, 32) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 2) {
                  conv.global[ramp(0, 1, 3)] = (conv.global[ramp(0, 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(3, 1, 3)] = (conv.global[ramp(3, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 1)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(6, 1, 3)] = (conv.global[ramp(6, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 2)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(9, 1, 3)] = (conv.global[ramp(9, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 3)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(12, 1, 3)] = (conv.global[ramp(12, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 4)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(15, 1, 3)] = (conv.global[ramp(15, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 5)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(18, 1, 3)] = (conv.global[ramp(18, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 6)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(21, 1, 3)] = (conv.global[ramp(21, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 7)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(24, 1, 3)] = (conv.global[ramp(24, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 8)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(27, 1, 3)] = (conv.global[ramp(27, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 9)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(30, 1, 3)] = (conv.global[ramp(30, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 10)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(33, 1, 3)] = (conv.global[ramp(33, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 11)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(36, 1, 3)] = (conv.global[ramp(36, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 12)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(39, 1, 3)] = (conv.global[ramp(39, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 13)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(42, 1, 3)] = (conv.global[ramp(42, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 14)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(45, 1, 3)] = (conv.global[ramp(45, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 15)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(48, 1, 3)] = (conv.global[ramp(48, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 16)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(51, 1, 3)] = (conv.global[ramp(51, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 17)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(54, 1, 3)] = (conv.global[ramp(54, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 18)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(57, 1, 3)] = (conv.global[ramp(57, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 19)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(60, 1, 3)] = (conv.global[ramp(60, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 20)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(63, 1, 3)] = (conv.global[ramp(63, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 21)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(66, 1, 3)] = (conv.global[ramp(66, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 22)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(69, 1, 3)] = (conv.global[ramp(69, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 23)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(72, 1, 3)] = (conv.global[ramp(72, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 24)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(75, 1, 3)] = (conv.global[ramp(75, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 25)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(78, 1, 3)] = (conv.global[ramp(78, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 26)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(81, 1, 3)] = (conv.global[ramp(81, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 27)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(84, 1, 3)] = (conv.global[ramp(84, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 28)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                  conv.global[ramp(87, 1, 3)] = (conv.global[ramp(87, 1, 3)] + (x3(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*2) + ic.inner)*62) + (ow.outer*30)) + kw) + 29)]))*int64x3(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*32) + ic.outer)*3) + kh)*3) + kw)*2) + ic.inner)*3), 1, 3)])))
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp((((ow.outer*30) + ow.inner)*3), 1, 3)] = conv.global[ramp((ow.inner*3), 1, 3)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.fused/32)*96) + (c.outer.h.fused % 32))*2) + w.outer)*30) + w.inner), 1920, 3)] = conv[ramp((((w.outer*30) + w.inner)*3), 1, 3)]
      }
    }
  }
}
