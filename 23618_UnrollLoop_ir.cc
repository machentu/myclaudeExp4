// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 43520]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 39168]
produce data_vec {
  parallel (C.h.fused, 0, 1280) {
    for (c, 0, 2) {
      for (w, 0, 17) {
        data_vec[(((C.h.fused*34) + (c*17)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 10)) && ((C.h.fused % 10) < 9)) && (1 <= w)) && (w < 16)), placeholder[((((((C.h.fused/10)*240) + (c*120)) + ((C.h.fused % 10)*15)) + w) - 16)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 128) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          kernel_vec[ramp(((((((CO.h.fused/3)*6912) + (CI*54)) + ((CO.h.fused % 3)*18)) + (w*6)) + (ci*3)), 1, 3)] = placeholder[ramp(((((((CO.h.fused/3)*6912) + (CI*18)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 2304, 3)]
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
        for (ic.outer, 0, 128) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 2) {
                conv.global[ramp((0*3), 1, 3)] = (conv.global[ramp((0*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 0) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((1*3), 1, 3)] = (conv.global[ramp((1*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 1) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((2*3), 1, 3)] = (conv.global[ramp((2*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 2) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((3*3), 1, 3)] = (conv.global[ramp((3*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 3) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((4*3), 1, 3)] = (conv.global[ramp((4*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 4) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((5*3), 1, 3)] = (conv.global[ramp((5*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 5) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((6*3), 1, 3)] = (conv.global[ramp((6*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 6) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((7*3), 1, 3)] = (conv.global[ramp((7*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 7) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((8*3), 1, 3)] = (conv.global[ramp((8*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 8) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((9*3), 1, 3)] = (conv.global[ramp((9*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 9) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((10*3), 1, 3)] = (conv.global[ramp((10*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 10) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((11*3), 1, 3)] = (conv.global[ramp((11*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 11) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((12*3), 1, 3)] = (conv.global[ramp((12*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 12) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((13*3), 1, 3)] = (conv.global[ramp((13*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 13) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                conv.global[ramp((14*3), 1, 3)] = (conv.global[ramp((14*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*340) + (kh*34)) + ((c.outer.h.fused % 8)*34)) + (ic.inner*17)) + 14) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/8)*6912) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
              }
            }
          }
        }
      }
      for (ow.inner, 0, 15) {
        conv.global[ramp((ow.inner*3), 1, 3)] = conv.global[ramp((ow.inner*3), 1, 3)]
      }
    }
    for (w.inner, 0, 15) {
      output_unpack[ramp(((((c.outer.h.fused/8)*360) + ((c.outer.h.fused % 8)*15)) + w.inner), 120, 3)] = conv.global[ramp((w.inner*3), 1, 3)]
    }
  }
}
