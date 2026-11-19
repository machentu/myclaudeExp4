// attr [data_vec] storage_scope = "global"
allocate data_vec[uint8 * 1 * 1 * 257 * 3 * 481]
produce data_vec {
  parallel (C.h.fused, 0, 257) {
    for (c, 0, 3) {
      for (w, 0, 481) {
        data_vec[(((C.h.fused*1443) + (c*481)) + w)] = tvm_if_then_else(((1 <= C.h.fused) && (1 <= w)), placeholder[((((c*122880) + (C.h.fused*480)) + w) - 481)], (uint8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 2 * 1 * 3 * 3 * 3 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 6) {
    for (w, 0, 3) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((CO.h.fused*72) + (w*24)) + (ci*8)), 1, 8)] = placeholder[ramp((((((CO.h.fused/3)*216) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 27, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 256) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 240 * 8]
    produce conv {
      for (ow.outer, 0, 8) {
        // attr [conv.global] storage_scope = "global"
        allocate conv.global[int64 * 1 * 1 * 1 * 30 * 8]
        produce conv.global {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
          }
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 3) {
                unrolled (ow.c.inner, 0, 30) {
                  conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[(((((((c.outer.h.fused % 128)*2886) + (kh*1443)) + (ic.inner*481)) + (ow.outer*60)) + (ow.c.inner*2)) + kw)]))*int64x8(kernel_vec[ramp((((((c.outer.h.fused/128)*216) + (kh*72)) + (kw*24)) + (ic.inner*8)), 1, 8)])))
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
    for (w.outer, 0, 8) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.fused/128)*245760) + ((c.outer.h.fused % 128)*240)) + (w.outer*30)) + w.inner), 30720, 8)] = conv[ramp(((w.outer*240) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
