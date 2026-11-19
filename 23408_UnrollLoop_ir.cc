// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 24480]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 3) {
      for (w, 0, 30) {
        data_vec[(((C.h.fused*90) + (c*30)) + w)] = placeholder[(((((C.h.fused/16)*1440) + (c*480)) + ((C.h.fused % 16)*30)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((CO.h.fused*153) + (CI*9)) + (ci*3)), 1, 3)] = placeholder[ramp((((CO.h.fused*153) + (CI*3)) + ci), 51, 3)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 272) {
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
        for (ic.outer, 0, 17) {
          for (ic.inner, 0, 3) {
            conv.global[ramp((0*3), 1, 3)] = (conv.global[ramp((0*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 0)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((1*3), 1, 3)] = (conv.global[ramp((1*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 1)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((2*3), 1, 3)] = (conv.global[ramp((2*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 2)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((3*3), 1, 3)] = (conv.global[ramp((3*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 3)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((4*3), 1, 3)] = (conv.global[ramp((4*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 4)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((5*3), 1, 3)] = (conv.global[ramp((5*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 5)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((6*3), 1, 3)] = (conv.global[ramp((6*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 6)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((7*3), 1, 3)] = (conv.global[ramp((7*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 7)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((8*3), 1, 3)] = (conv.global[ramp((8*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 8)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((9*3), 1, 3)] = (conv.global[ramp((9*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 9)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((10*3), 1, 3)] = (conv.global[ramp((10*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 10)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((11*3), 1, 3)] = (conv.global[ramp((11*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 11)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((12*3), 1, 3)] = (conv.global[ramp((12*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 12)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((13*3), 1, 3)] = (conv.global[ramp((13*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 13)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((14*3), 1, 3)] = (conv.global[ramp((14*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 14)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((15*3), 1, 3)] = (conv.global[ramp((15*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 15)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((16*3), 1, 3)] = (conv.global[ramp((16*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 16)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((17*3), 1, 3)] = (conv.global[ramp((17*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 17)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((18*3), 1, 3)] = (conv.global[ramp((18*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 18)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((19*3), 1, 3)] = (conv.global[ramp((19*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 19)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((20*3), 1, 3)] = (conv.global[ramp((20*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 20)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((21*3), 1, 3)] = (conv.global[ramp((21*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 21)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((22*3), 1, 3)] = (conv.global[ramp((22*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 22)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((23*3), 1, 3)] = (conv.global[ramp((23*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 23)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((24*3), 1, 3)] = (conv.global[ramp((24*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 24)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((25*3), 1, 3)] = (conv.global[ramp((25*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 25)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((26*3), 1, 3)] = (conv.global[ramp((26*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 26)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((27*3), 1, 3)] = (conv.global[ramp((27*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 27)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((28*3), 1, 3)] = (conv.global[ramp((28*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 28)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp((29*3), 1, 3)] = (conv.global[ramp((29*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + 29)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
          }
        }
      }
      for (ow, 0, 30) {
        conv.global[ramp((ow*3), 1, 3)] = conv.global[ramp((ow*3), 1, 3)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.outer.fused/16)*1440) + ((c.outer.h.outer.fused % 16)*30)) + w.inner), 480, 3)] = conv.global[ramp((w.inner*3), 1, 3)]
    }
  }
}
