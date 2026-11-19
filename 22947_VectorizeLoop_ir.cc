// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 4 * 10 * 8 * 17]
produce data_vec {
  parallel (C.h.fused, 0, 40) {
    for (c, 0, 8) {
      for (w, 0, 17) {
        data_vec[(((C.h.fused*136) + (c*17)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 10)) && ((C.h.fused % 10) < 9)) && (1 <= w)) && (w < 16)), placeholder[((((((C.h.fused/10)*960) + (c*120)) + ((C.h.fused % 10)*15)) + w) - 16)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 4 * 4 * 3 * 3 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 12) {
    for (CI, 0, 4) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((CO.h.fused/3)*2304) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)), 1, 8)] = placeholder[ramp(((((((CO.h.fused/3)*2304) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 288, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 32) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 15 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 15 * 8]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 15) {
          conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
        }
        for (ic.outer, 0, 4) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                unrolled (ow.c.inner, 0, 15) {
                  conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*1360) + (kh*136)) + ((c.outer.h.fused % 8)*136)) + (ic.inner*17)) + ow.c.inner) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/8)*2304) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 15) {
        conv[ramp((ow.inner*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
      }
    }
    for (w.inner, 0, 15) {
      output_unpack[ramp(((((c.outer.h.fused/8)*960) + ((c.outer.h.fused % 8)*15)) + w.inner), 120, 8)] = conv[ramp((w.inner*8), 1, 8)]
    }
  }
}
