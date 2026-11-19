// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 128832]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 4608]
produce data_vec {
  parallel (C.h.fused, 0, 264) {
    for (c, 0, 8) {
      for (w, 0, 61) {
        data_vec[(((C.h.fused*488) + (c*61)) + w)] = tvm_if_then_else(((1 <= (C.h.fused % 33)) && (1 <= w)), placeholder[((((((C.h.fused/33)*15360) + (c*1920)) + ((C.h.fused % 33)*60)) + w) - 61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 24) {
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
  parallel (c.outer.h.fused, 0, 128) {
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
        for (ic.outer, 0, 8) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                conv.global[ramp((0*8), 1, 8)] = (conv.global[ramp((0*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (0*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((1*8), 1, 8)] = (conv.global[ramp((1*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (1*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((2*8), 1, 8)] = (conv.global[ramp((2*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (2*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((3*8), 1, 8)] = (conv.global[ramp((3*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (3*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((4*8), 1, 8)] = (conv.global[ramp((4*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (4*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((5*8), 1, 8)] = (conv.global[ramp((5*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (5*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((6*8), 1, 8)] = (conv.global[ramp((6*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (6*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((7*8), 1, 8)] = (conv.global[ramp((7*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (7*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((8*8), 1, 8)] = (conv.global[ramp((8*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (8*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((9*8), 1, 8)] = (conv.global[ramp((9*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (9*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((10*8), 1, 8)] = (conv.global[ramp((10*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (10*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((11*8), 1, 8)] = (conv.global[ramp((11*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (11*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((12*8), 1, 8)] = (conv.global[ramp((12*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (12*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((13*8), 1, 8)] = (conv.global[ramp((13*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (13*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((14*8), 1, 8)] = (conv.global[ramp((14*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (14*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((15*8), 1, 8)] = (conv.global[ramp((15*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (15*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((16*8), 1, 8)] = (conv.global[ramp((16*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (16*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((17*8), 1, 8)] = (conv.global[ramp((17*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (17*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((18*8), 1, 8)] = (conv.global[ramp((18*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (18*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((19*8), 1, 8)] = (conv.global[ramp((19*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (19*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((20*8), 1, 8)] = (conv.global[ramp((20*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (20*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((21*8), 1, 8)] = (conv.global[ramp((21*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (21*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((22*8), 1, 8)] = (conv.global[ramp((22*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (22*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((23*8), 1, 8)] = (conv.global[ramp((23*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (23*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((24*8), 1, 8)] = (conv.global[ramp((24*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (24*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((25*8), 1, 8)] = (conv.global[ramp((25*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (25*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((26*8), 1, 8)] = (conv.global[ramp((26*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (26*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((27*8), 1, 8)] = (conv.global[ramp((27*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (27*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((28*8), 1, 8)] = (conv.global[ramp((28*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (28*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((29*8), 1, 8)] = (conv.global[ramp((29*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*16104) + ((c.outer.h.fused % 16)*976)) + (kh*488)) + (ic.inner*61)) + (29*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
              }
            }
          }
        }
      }
      for (ow.inner, 0, 30) {
        conv.global[ramp((ow.inner*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.fused/16)*3840) + ((c.outer.h.fused % 16)*30)) + w.inner), 480, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
