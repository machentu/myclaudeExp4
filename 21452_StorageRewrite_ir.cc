// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 36864]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 4608]
produce data_vec {
  parallel (C.h.fused, 0, 144) {
    for (c, 0, 8) {
      for (w, 0, 32) {
        data_vec[(((C.h.fused*256) + (c*32)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 18)) && ((C.h.fused % 18) < 17)) && (1 <= w)) && (w < 31)), placeholder[((((((C.h.fused/18)*3840) + (c*480)) + ((C.h.fused % 18)*30)) + w) - 31)], (int8)0)
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
        unrolled (ow.c.inner.init, 0, 30) {
          conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
        }
        for (ic.outer, 0, 8) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 8) {
                unrolled (ow.c.inner, 0, 30) {
                  conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4608) + (kh*256)) + ((c.outer.h.fused % 16)*256)) + (ic.inner*32)) + ow.c.inner) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/16)*4608) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                }
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
