// attr [data_vec] storage_scope = "global"
allocate data_vec[uint8 * 27531]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 1176]
produce data_vec {
  parallel (C.h.fused, 0, 133) {
    for (c, 0, 3) {
      for (w, 0, 69) {
        data_vec[((((C.h.fused*3) + c)*69) + w)] = tvm_if_then_else(((((3 <= C.h.fused) && (C.h.fused < 131)) && (3 <= w)) && (w < 67)), placeholder[(((((c*128) + C.h.fused)*64) + w) + -195)], (uint8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 56) {
    for (w, 0, 7) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((((CO.h.fused*7) + w)*3) + ci)*8), 1, 8)] = placeholder[ramp((((((((CO.h.fused/7)*24) + ci)*7) + (CO.h.fused % 7))*7) + w), 147, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 512) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x8 * 32]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 16]
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
          for (kh, 0, 7) {
            for (kw, 0, 7) {
              for (ic.inner, 0, 3) {
                conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 2)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 4)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 6)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 8)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 10)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 12)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 14)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 16)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 18)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 20)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 22)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 24)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 26)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 28)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
                conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[(((((((((c.outer.h.fused % 64)*2) + kh)*3) + ic.inner)*69) + (ow.outer*32)) + kw) + 30)]))*int64x8(kernel_vec[ramp(((((((((c.outer.h.fused/64)*7) + kh)*7) + kw)*3) + ic.inner)*8), 1, 8)])))
              }
            }
          }
        }
        for (ow.inner, 0, 16) {
          conv[ramp((((ow.outer*16) + ow.inner)*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 16) {
        output_unpack[ramp((((((((c.outer.h.fused/64)*512) + (c.outer.h.fused % 64))*2) + w.outer)*16) + w.inner), 2048, 8)] = conv[ramp((((w.outer*16) + w.inner)*8), 1, 8)]
      }
    }
  }
}
