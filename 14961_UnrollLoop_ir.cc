// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 32768]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 512]
produce data_vec {
  parallel (C.h.fused, 0, 256) {
    for (c, 0, 8) {
      for (w, 0, 16) {
        data_vec[(((C.h.fused*128) + (c*16)) + w)] = placeholder[(((((C.h.fused/32)*4096) + (c*512)) + ((C.h.fused % 32)*16)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 8) {
    for (CI, 0, 8) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*512) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*512) + (CI*8)) + ci), 64, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 16]
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
        for (ic.outer, 0, 8) {
          for (ic.inner, 0, 8) {
            conv.global[ramp((0*8), 1, 8)] = (conv.global[ramp((0*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((1*8), 1, 8)] = (conv.global[ramp((1*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((2*8), 1, 8)] = (conv.global[ramp((2*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((3*8), 1, 8)] = (conv.global[ramp((3*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((4*8), 1, 8)] = (conv.global[ramp((4*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((5*8), 1, 8)] = (conv.global[ramp((5*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((6*8), 1, 8)] = (conv.global[ramp((6*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((7*8), 1, 8)] = (conv.global[ramp((7*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((8*8), 1, 8)] = (conv.global[ramp((8*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 8)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((9*8), 1, 8)] = (conv.global[ramp((9*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 9)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((10*8), 1, 8)] = (conv.global[ramp((10*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 10)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((11*8), 1, 8)] = (conv.global[ramp((11*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 11)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((12*8), 1, 8)] = (conv.global[ramp((12*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 12)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((13*8), 1, 8)] = (conv.global[ramp((13*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 13)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((14*8), 1, 8)] = (conv.global[ramp((14*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 14)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp((15*8), 1, 8)] = (conv.global[ramp((15*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + 15)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
          }
        }
      }
      for (ow, 0, 16) {
        conv.global[ramp((ow*8), 1, 8)] = conv.global[ramp((ow*8), 1, 8)]
      }
    }
    for (w.inner, 0, 16) {
      output_unpack[ramp(((((c.outer.h.outer.fused/32)*4096) + ((c.outer.h.outer.fused % 32)*16)) + w.inner), 512, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
