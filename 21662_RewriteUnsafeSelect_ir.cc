// attr [data_vec] storage_scope = "global"
allocate data_vec[uint8 * 370851]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 54]
produce data_vec {
  parallel (C.h.fused, 0, 257) {
    for (c, 0, 3) {
      for (w, 0, 481) {
        data_vec[((((C.h.fused*3) + c)*481) + w)] = tvm_if_then_else((1 <= min(C.h.fused, w)), placeholder[(((((c*256) + C.h.fused)*480) + w) + -481)], (uint8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 6) {
    for (w, 0, 3) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((((CO.h.fused*3) + w)*3) + ci)*8), 1, 8)] = placeholder[ramp((((((((CO.h.fused/3)*24) + ci)*3) + (CO.h.fused % 3))*3) + w), 27, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x8 * 240]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      for (ow.outer, 0, 8) {
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
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 3) {
                conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 2)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 4)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 6)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 8)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 10)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 12)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 14)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 16)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 18)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 20)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 22)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 24)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 26)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 28)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 30)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 32)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 34)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 36)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 38)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 40)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 42)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 44)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 46)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 48)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 50)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 52)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 54)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 56)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 128)*2) + kh)*3) + ic.inner)*481) + (ow.outer*60)) + kw) + 58)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/128)*3) + kh)*3) + kw)*3) + ic.inner)*8), 1, 8)])))
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp((((ow.outer*30) + ow.inner)*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
        }
      }
    }
    for (w.outer, 0, 8) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.fused/128)*1024) + (c.outer.h.fused % 128))*8) + w.outer)*30) + w.inner), 30720, 8)] = conv[ramp((((w.outer*30) + w.inner)*8), 1, 8)]
      }
    }
  }
}
