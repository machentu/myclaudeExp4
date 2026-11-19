// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 497424]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 576]
produce data_vec {
  parallel (C.h.fused, 0, 258) {
    for (c, 0, 8) {
      for (w, 0, 241) {
        data_vec[(((C.h.fused*1928) + (c*241)) + w)] = tvm_if_then_else(((1 <= (C.h.fused % 129)) && (1 <= w)), placeholder[((((((C.h.fused/129)*245760) + (c*30720)) + ((C.h.fused % 129)*240)) + w) - 241)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 12) {
    for (CI, 0, 2) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((CO.h.fused/3)*1152) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)), 1, 8)] = placeholder[ramp(((((((CO.h.fused/3)*1152) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 144, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x8 * 120]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      for (ow.outer, 0, 4) {
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
          for (ic.outer, 0, 2) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 8) {
                  conv.global[ramp((0*8), 1, 8)] = (conv.global[ramp((0*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (0*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((1*8), 1, 8)] = (conv.global[ramp((1*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (1*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((2*8), 1, 8)] = (conv.global[ramp((2*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (2*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((3*8), 1, 8)] = (conv.global[ramp((3*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (3*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((4*8), 1, 8)] = (conv.global[ramp((4*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (4*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((5*8), 1, 8)] = (conv.global[ramp((5*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (5*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((6*8), 1, 8)] = (conv.global[ramp((6*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (6*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((7*8), 1, 8)] = (conv.global[ramp((7*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (7*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((8*8), 1, 8)] = (conv.global[ramp((8*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (8*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((9*8), 1, 8)] = (conv.global[ramp((9*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (9*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((10*8), 1, 8)] = (conv.global[ramp((10*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (10*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((11*8), 1, 8)] = (conv.global[ramp((11*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (11*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((12*8), 1, 8)] = (conv.global[ramp((12*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (12*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((13*8), 1, 8)] = (conv.global[ramp((13*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (13*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((14*8), 1, 8)] = (conv.global[ramp((14*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (14*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((15*8), 1, 8)] = (conv.global[ramp((15*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (15*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((16*8), 1, 8)] = (conv.global[ramp((16*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (16*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((17*8), 1, 8)] = (conv.global[ramp((17*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (17*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((18*8), 1, 8)] = (conv.global[ramp((18*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (18*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((19*8), 1, 8)] = (conv.global[ramp((19*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (19*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((20*8), 1, 8)] = (conv.global[ramp((20*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (20*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((21*8), 1, 8)] = (conv.global[ramp((21*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (21*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((22*8), 1, 8)] = (conv.global[ramp((22*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (22*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((23*8), 1, 8)] = (conv.global[ramp((23*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (23*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((24*8), 1, 8)] = (conv.global[ramp((24*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (24*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((25*8), 1, 8)] = (conv.global[ramp((25*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (25*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((26*8), 1, 8)] = (conv.global[ramp((26*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (26*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((27*8), 1, 8)] = (conv.global[ramp((27*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (27*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((28*8), 1, 8)] = (conv.global[ramp((28*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (28*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  conv.global[ramp((29*8), 1, 8)] = (conv.global[ramp((29*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (29*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
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
    for (w.outer, 0, 4) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.fused/64)*61440) + ((c.outer.h.fused % 64)*120)) + (w.outer*30)) + w.inner), 7680, 8)] = conv[ramp(((w.outer*240) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
