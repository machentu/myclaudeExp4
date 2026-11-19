// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 67456]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 36864]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 8) {
      for (w, 0, 31) {
        data_vec[(((C.h.fused*248) + (c*31)) + w)] = tvm_if_then_else(((1 <= (C.h.fused % 17)) && (1 <= w)), placeholder[((((((C.h.fused/17)*3840) + (c*480)) + ((C.h.fused % 17)*30)) + w) - 31)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 96) {
    for (CI, 0, 16) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((CO.h.fused/3)*9216) + (CI*576)) + ((CO.h.fused % 3)*192)) + (w*64)) + (ci*8)), 1, 8)] = placeholder[ramp(((((((CO.h.fused/3)*9216) + (CI*72)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 1152, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 256) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 15]
    produce conv {
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 15) {
          conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
        }
        for (ic.outer, 0, 16) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                unrolled (ow.c.inner, 0, 15) {
                  conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4216) + ((c.outer.h.fused % 8)*496)) + (kh*248)) + (ic.inner*31)) + (ow.c.inner*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/8)*9216) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 15) {
        conv.global[ramp((ow.inner*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
      }
    }
    for (w.inner, 0, 15) {
      output_unpack[ramp(((((c.outer.h.fused/8)*960) + ((c.outer.h.fused % 8)*15)) + w.inner), 120, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
