// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 134912]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 5760]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 8) {
      for (w, 0, 62) {
        data_vec[(((C.h.fused*496) + (c*62)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((C.h.fused/34)*15360) + (c*1920)) + ((C.h.fused % 34)*60)) + w) - 61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 30) {
    for (CI, 0, 8) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((CO.h.fused/3)*4608) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)), 1, 8)] = placeholder[ramp(((((((CO.h.fused/3)*4608) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 576, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 320) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x8 * 60]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      for (ow.outer, 0, 2) {
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
          for (ic.outer, 0, 8) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 8) {
                  conv.global[ramp((0*8), 1, 8)] = (conv.global[ramp((0*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 0) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((1*8), 1, 8)] = (conv.global[ramp((1*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 1) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((2*8), 1, 8)] = (conv.global[ramp((2*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 2) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((3*8), 1, 8)] = (conv.global[ramp((3*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 3) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((4*8), 1, 8)] = (conv.global[ramp((4*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 4) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((5*8), 1, 8)] = (conv.global[ramp((5*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 5) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((6*8), 1, 8)] = (conv.global[ramp((6*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 6) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((7*8), 1, 8)] = (conv.global[ramp((7*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 7) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((8*8), 1, 8)] = (conv.global[ramp((8*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 8) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((9*8), 1, 8)] = (conv.global[ramp((9*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 9) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((10*8), 1, 8)] = (conv.global[ramp((10*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 10) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((11*8), 1, 8)] = (conv.global[ramp((11*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 11) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((12*8), 1, 8)] = (conv.global[ramp((12*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 12) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((13*8), 1, 8)] = (conv.global[ramp((13*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 13) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((14*8), 1, 8)] = (conv.global[ramp((14*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 14) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((15*8), 1, 8)] = (conv.global[ramp((15*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 15) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((16*8), 1, 8)] = (conv.global[ramp((16*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 16) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((17*8), 1, 8)] = (conv.global[ramp((17*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 17) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((18*8), 1, 8)] = (conv.global[ramp((18*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 18) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((19*8), 1, 8)] = (conv.global[ramp((19*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 19) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((20*8), 1, 8)] = (conv.global[ramp((20*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 20) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((21*8), 1, 8)] = (conv.global[ramp((21*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 21) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((22*8), 1, 8)] = (conv.global[ramp((22*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 22) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((23*8), 1, 8)] = (conv.global[ramp((23*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 23) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((24*8), 1, 8)] = (conv.global[ramp((24*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 24) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((25*8), 1, 8)] = (conv.global[ramp((25*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 25) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((26*8), 1, 8)] = (conv.global[ramp((26*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 26) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((27*8), 1, 8)] = (conv.global[ramp((27*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 27) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((28*8), 1, 8)] = (conv.global[ramp((28*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 28) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((29*8), 1, 8)] = (conv.global[ramp((29*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*16864) + (kh*496)) + ((c.outer.h.fused % 32)*496)) + (ic.inner*62)) + (ow.outer*30)) + 29) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/32)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp(((ow.outer*240) + (ow.inner*8)), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.fused/32)*15360) + ((c.outer.h.fused % 32)*60)) + (w.outer*30)) + w.inner), 1920, 8)] = conv[ramp(((w.outer*240) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
