// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 67456]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 1152]
produce data_vec {
  parallel (C.h.fused, 0, 136) {
    for (c, 0, 8) {
      for (w, 0, 62) {
        data_vec[((((C.h.fused*8) + c)*62) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((((C.h.fused/34)*8) + c)*32) + (C.h.fused % 34))*60) + w) + -61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 12) {
    for (CI, 0, 4) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((((((CO.h.fused/3)*4) + CI)*3) + (CO.h.fused % 3))*3) + w)*8) + ci)*8), 1, 8)] = placeholder[ramp((((((((((CO.h.fused/3)*32) + CI)*8) + ci)*3) + (CO.h.fused % 3))*3) + w), 288, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 128) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x8 * 60]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      for (ow.outer, 0, 2) {
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
          for (ic.outer, 0, 4) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 8) {
                  conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 1)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 2)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 3)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 4)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 5)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 6)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 7)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 8)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 9)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 10)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 11)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 12)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 13)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 14)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 15)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 16)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 17)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 18)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 19)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 20)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 21)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 22)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 23)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 24)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 25)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 26)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 27)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 28)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 29)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*4) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp((((ow.outer*30) + ow.inner)*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.fused/32)*256) + (c.outer.h.fused % 32))*2) + w.outer)*30) + w.inner), 1920, 8)] = conv[ramp((((w.outer*30) + w.inner)*8), 1, 8)]
      }
    }
  }
}
