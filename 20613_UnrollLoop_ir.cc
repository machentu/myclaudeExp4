// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 61440]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 2048]
produce data_vec {
  parallel (C.h.fused, 0, 256) {
    for (c, 0, 8) {
      for (w, 0, 30) {
        data_vec[(((C.h.fused*240) + (c*30)) + w)] = placeholder[(((((C.h.fused/16)*3840) + (c*480)) + ((C.h.fused % 16)*30)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 16) {
    for (CI, 0, 16) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*1024) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*1024) + (CI*8)) + ci), 128, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      produce conv.global {
        conv.global[ramp((0*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((1*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((2*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((3*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((4*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((5*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((6*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((7*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((8*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((9*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((10*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((11*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((12*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((13*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((14*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((15*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((16*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((17*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((18*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((19*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((20*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((21*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((22*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((23*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((24*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((25*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((26*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((27*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((28*8), 1, 8)] = x8((int64)0)
        conv.global[ramp((29*8), 1, 8)] = x8((int64)0)
        for (ic.outer, 0, 16) {
          for (ic.inner, 0, 8) {
            conv.global[ramp((0*8), 1, 8)] = (conv.global[ramp((0*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((1*8), 1, 8)] = (conv.global[ramp((1*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((2*8), 1, 8)] = (conv.global[ramp((2*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((3*8), 1, 8)] = (conv.global[ramp((3*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((4*8), 1, 8)] = (conv.global[ramp((4*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((5*8), 1, 8)] = (conv.global[ramp((5*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((6*8), 1, 8)] = (conv.global[ramp((6*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((7*8), 1, 8)] = (conv.global[ramp((7*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((8*8), 1, 8)] = (conv.global[ramp((8*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 8)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((9*8), 1, 8)] = (conv.global[ramp((9*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 9)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((10*8), 1, 8)] = (conv.global[ramp((10*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 10)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((11*8), 1, 8)] = (conv.global[ramp((11*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 11)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((12*8), 1, 8)] = (conv.global[ramp((12*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 12)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((13*8), 1, 8)] = (conv.global[ramp((13*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 13)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((14*8), 1, 8)] = (conv.global[ramp((14*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 14)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((15*8), 1, 8)] = (conv.global[ramp((15*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 15)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((16*8), 1, 8)] = (conv.global[ramp((16*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 16)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((17*8), 1, 8)] = (conv.global[ramp((17*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 17)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((18*8), 1, 8)] = (conv.global[ramp((18*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 18)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((19*8), 1, 8)] = (conv.global[ramp((19*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 19)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((20*8), 1, 8)] = (conv.global[ramp((20*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 20)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((21*8), 1, 8)] = (conv.global[ramp((21*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 21)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((22*8), 1, 8)] = (conv.global[ramp((22*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 22)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((23*8), 1, 8)] = (conv.global[ramp((23*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 23)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((24*8), 1, 8)] = (conv.global[ramp((24*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 24)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((25*8), 1, 8)] = (conv.global[ramp((25*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 25)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((26*8), 1, 8)] = (conv.global[ramp((26*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 26)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((27*8), 1, 8)] = (conv.global[ramp((27*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 27)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((28*8), 1, 8)] = (conv.global[ramp((28*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 28)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((29*8), 1, 8)] = (conv.global[ramp((29*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + 29)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
          }
        }
      }
      for (ow, 0, 30) {
        conv.global[ramp((ow*8), 1, 8)] = conv.global[ramp((ow*8), 1, 8)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.outer.fused/16)*3840) + ((c.outer.h.outer.fused % 16)*30)) + w.inner), 480, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
