// attr [data_vec] storage_scope = "global"
allocate data_vec[uint8 * 27531]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 1176]
produce data_vec {
  parallel (C.h.fused, 0, 133) {
    for (c, 0, 3) {
      for (w, 0, 69) {
        data_vec[(((C.h.fused*207) + (c*69)) + w)] = tvm_if_then_else(((((3 <= C.h.fused) && (C.h.fused < 131)) && (3 <= w)) && (w < 67)), placeholder[((((c*8192) + (C.h.fused*64)) + w) - 195)], (uint8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 56) {
    for (w, 0, 7) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((CO.h.fused*168) + (w*24)) + (ci*8)), 1, 8)] = placeholder[ramp((((((CO.h.fused/7)*1176) + (ci*49)) + ((CO.h.fused % 7)*7)) + w), 147, 8)]
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
          for (kh, 0, 7) {
            for (kw, 0, 7) {
              for (ic.inner, 0, 3) {
                conv.global[ramp((0*8), 1, 8)] = (conv.global[ramp((0*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (0*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((1*8), 1, 8)] = (conv.global[ramp((1*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (1*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((2*8), 1, 8)] = (conv.global[ramp((2*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (2*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((3*8), 1, 8)] = (conv.global[ramp((3*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (3*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((4*8), 1, 8)] = (conv.global[ramp((4*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (4*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((5*8), 1, 8)] = (conv.global[ramp((5*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (5*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((6*8), 1, 8)] = (conv.global[ramp((6*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (6*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((7*8), 1, 8)] = (conv.global[ramp((7*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (7*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((8*8), 1, 8)] = (conv.global[ramp((8*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (8*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((9*8), 1, 8)] = (conv.global[ramp((9*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (9*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((10*8), 1, 8)] = (conv.global[ramp((10*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (10*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((11*8), 1, 8)] = (conv.global[ramp((11*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (11*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((12*8), 1, 8)] = (conv.global[ramp((12*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (12*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((13*8), 1, 8)] = (conv.global[ramp((13*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (13*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((14*8), 1, 8)] = (conv.global[ramp((14*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (14*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
                conv.global[ramp((15*8), 1, 8)] = (conv.global[ramp((15*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 64)*414) + (kh*207)) + (ic.inner*69)) + (ow.outer*32)) + (15*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/64)*1176) + (kh*168)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
              }
            }
          }
        }
        for (ow.inner, 0, 16) {
          conv[ramp(((ow.outer*128) + (ow.inner*8)), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 16) {
        output_unpack[ramp((((((c.outer.h.fused/64)*16384) + ((c.outer.h.fused % 64)*32)) + (w.outer*16)) + w.inner), 2048, 8)] = conv[ramp(((w.outer*128) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
