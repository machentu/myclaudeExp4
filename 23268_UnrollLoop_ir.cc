// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 134912]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 9792]
produce data_vec {
  parallel (C.h.fused, 0, 1088) {
    for (c, 0, 2) {
      for (w, 0, 62) {
        data_vec[(((C.h.fused*124) + (c*62)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((C.h.fused/34)*3840) + (c*1920)) + ((C.h.fused % 34)*60)) + w) - 61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 32) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          kernel_vec[ramp(((((((CO.h.fused/3)*1728) + (CI*54)) + ((CO.h.fused % 3)*18)) + (w*6)) + (ci*3)), 1, 3)] = placeholder[ramp(((((((CO.h.fused/3)*1728) + (CI*18)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 576, 3)]
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
          for (ic.outer, 0, 32) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 2) {
                  conv.global[ramp((0*3), 1, 3)] = (conv.global[ramp((0*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 0) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((1*3), 1, 3)] = (conv.global[ramp((1*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 1) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((2*3), 1, 3)] = (conv.global[ramp((2*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 2) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((3*3), 1, 3)] = (conv.global[ramp((3*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 3) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((4*3), 1, 3)] = (conv.global[ramp((4*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 4) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((5*3), 1, 3)] = (conv.global[ramp((5*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 5) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((6*3), 1, 3)] = (conv.global[ramp((6*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 6) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((7*3), 1, 3)] = (conv.global[ramp((7*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 7) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((8*3), 1, 3)] = (conv.global[ramp((8*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 8) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((9*3), 1, 3)] = (conv.global[ramp((9*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 9) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((10*3), 1, 3)] = (conv.global[ramp((10*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 10) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((11*3), 1, 3)] = (conv.global[ramp((11*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 11) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((12*3), 1, 3)] = (conv.global[ramp((12*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 12) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((13*3), 1, 3)] = (conv.global[ramp((13*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 13) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((14*3), 1, 3)] = (conv.global[ramp((14*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 14) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((15*3), 1, 3)] = (conv.global[ramp((15*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 15) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((16*3), 1, 3)] = (conv.global[ramp((16*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 16) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((17*3), 1, 3)] = (conv.global[ramp((17*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 17) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((18*3), 1, 3)] = (conv.global[ramp((18*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 18) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((19*3), 1, 3)] = (conv.global[ramp((19*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 19) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((20*3), 1, 3)] = (conv.global[ramp((20*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 20) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((21*3), 1, 3)] = (conv.global[ramp((21*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 21) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((22*3), 1, 3)] = (conv.global[ramp((22*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 22) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((23*3), 1, 3)] = (conv.global[ramp((23*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 23) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((24*3), 1, 3)] = (conv.global[ramp((24*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 24) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((25*3), 1, 3)] = (conv.global[ramp((25*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 25) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((26*3), 1, 3)] = (conv.global[ramp((26*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 26) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((27*3), 1, 3)] = (conv.global[ramp((27*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 27) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((28*3), 1, 3)] = (conv.global[ramp((28*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 28) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  conv.global[ramp((29*3), 1, 3)] = (conv.global[ramp((29*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + 29) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp(((ow.outer*90) + (ow.inner*3)), 1, 3)] = conv.global[ramp((ow.inner*3), 1, 3)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.fused/32)*5760) + ((c.outer.h.fused % 32)*60)) + (w.outer*30)) + w.inner), 1920, 3)] = conv[ramp(((w.outer*90) + (w.inner*3)), 1, 3)]
      }
    }
  }
}
