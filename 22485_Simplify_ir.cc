// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 128832]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 4608]
produce data_vec {
  parallel (C.h.fused, 0, 264) {
    for (c, 0, 8) {
      for (w, 0, 61) {
        data_vec[((((C.h.fused*8) + c)*61) + w)] = tvm_if_then_else((1 <= min((C.h.fused % 33), w)), placeholder[((((((((C.h.fused/33)*8) + c)*32) + (C.h.fused % 33))*60) + w) + -61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 24) {
    for (CI, 0, 8) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((((((CO.h.fused/3)*8) + CI)*3) + (CO.h.fused % 3))*3) + w)*8) + ci)*8), 1, 8)] = placeholder[ramp((((((((((CO.h.fused/3)*64) + CI)*8) + ci)*3) + (CO.h.fused % 3))*3) + w), 576, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 128) {
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
        for (ic.outer, 0, 8) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 2)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 4)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 6)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 8)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 10)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 12)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 14)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 16)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 18)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 20)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 22)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 24)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 26)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 28)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 30)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 32)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 34)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 36)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 38)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 40)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 42)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 44)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 46)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 48)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 50)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 52)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 54)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 56)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + kw) + 58)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/16)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
              }
            }
          }
        }
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp((((((c.outer.h.fused/16)*128) + (c.outer.h.fused % 16))*30) + w.inner), 480, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
