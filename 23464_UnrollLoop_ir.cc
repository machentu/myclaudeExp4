// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 73728]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 19584]
produce data_vec {
  parallel (C.h.fused, 0, 1152) {
    for (c, 0, 2) {
      for (w, 0, 32) {
        data_vec[(((C.h.fused*64) + (c*32)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 18)) && ((C.h.fused % 18) < 17)) && (1 <= w)) && (w < 31)), placeholder[((((((C.h.fused/18)*960) + (c*480)) + ((C.h.fused % 18)*30)) + w) - 31)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 64) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          kernel_vec[ramp(((((((CO.h.fused/3)*3456) + (CI*54)) + ((CO.h.fused % 3)*18)) + (w*6)) + (ci*3)), 1, 3)] = placeholder[ramp(((((((CO.h.fused/3)*3456) + (CI*18)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 1152, 3)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 272) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 30]
    produce conv {
      produce conv.global {
        conv.global[ramp((0*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((1*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((2*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((3*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((4*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((5*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((6*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((7*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((8*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((9*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((10*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((11*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((12*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((13*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((14*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((15*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((16*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((17*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((18*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((19*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((20*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((21*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((22*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((23*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((24*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((25*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((26*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((27*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((28*3), 1, 3)] = x3((int64)0)
        conv.global[ramp((29*3), 1, 3)] = x3((int64)0)
        for (ic.outer, 0, 64) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 2) {
                conv.global[ramp((0*3), 1, 3)] = (conv.global[ramp((0*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 0) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((1*3), 1, 3)] = (conv.global[ramp((1*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 1) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((2*3), 1, 3)] = (conv.global[ramp((2*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 2) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((3*3), 1, 3)] = (conv.global[ramp((3*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 3) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((4*3), 1, 3)] = (conv.global[ramp((4*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 4) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((5*3), 1, 3)] = (conv.global[ramp((5*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 5) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((6*3), 1, 3)] = (conv.global[ramp((6*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 6) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((7*3), 1, 3)] = (conv.global[ramp((7*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 7) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((8*3), 1, 3)] = (conv.global[ramp((8*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 8) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((9*3), 1, 3)] = (conv.global[ramp((9*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 9) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((10*3), 1, 3)] = (conv.global[ramp((10*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 10) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((11*3), 1, 3)] = (conv.global[ramp((11*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 11) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((12*3), 1, 3)] = (conv.global[ramp((12*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 12) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((13*3), 1, 3)] = (conv.global[ramp((13*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 13) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((14*3), 1, 3)] = (conv.global[ramp((14*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 14) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((15*3), 1, 3)] = (conv.global[ramp((15*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 15) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((16*3), 1, 3)] = (conv.global[ramp((16*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 16) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((17*3), 1, 3)] = (conv.global[ramp((17*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 17) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((18*3), 1, 3)] = (conv.global[ramp((18*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 18) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((19*3), 1, 3)] = (conv.global[ramp((19*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 19) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((20*3), 1, 3)] = (conv.global[ramp((20*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 20) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((21*3), 1, 3)] = (conv.global[ramp((21*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 21) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((22*3), 1, 3)] = (conv.global[ramp((22*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 22) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((23*3), 1, 3)] = (conv.global[ramp((23*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 23) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((24*3), 1, 3)] = (conv.global[ramp((24*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 24) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((25*3), 1, 3)] = (conv.global[ramp((25*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 25) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((26*3), 1, 3)] = (conv.global[ramp((26*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 26) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((27*3), 1, 3)] = (conv.global[ramp((27*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 27) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((28*3), 1, 3)] = (conv.global[ramp((28*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 28) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((29*3), 1, 3)] = (conv.global[ramp((29*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1152) + (kh*64)) + ((c.outer.h.fused % 16)*64)) + (ic.inner*32)) + 29) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*3456) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
              }
            }
          }
        }
      }
      for (ow.inner, 0, 30) {
        conv.global[ramp((ow.inner*3), 1, 3)] = conv.global[ramp((ow.inner*3), 1, 3)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.fused/16)*1440) + ((c.outer.h.fused % 16)*30)) + w.inner), 480, 3)] = conv.global[ramp((w.inner*3), 1, 3)]
    }
  }
}
