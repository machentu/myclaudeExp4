// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 2 * 129 * 8 * 241]
produce data_vec {
  parallel (C.h.fused, 0, 258) {
    for (c, 0, 8) {
      for (w, 0, 241) {
        data_vec[(((C.h.fused*1928) + (c*241)) + w)] = tvm_if_then_else(((1 <= (C.h.fused % 129)) && (1 <= w)), placeholder[((((((C.h.fused/129)*245760) + (c*30720)) + ((C.h.fused % 129)*240)) + w) - 241)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 4 * 2 * 3 * 3 * 8 * 8]
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
    allocate conv[int64 * 1 * 1 * 1 * 120 * 8]
    produce conv {
      for (ow.outer, 0, 4) {
        // attr [conv.global] storage_scope = "global"
        allocate conv.global[int64 * 1 * 1 * 1 * 30 * 8]
        produce conv.global {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
          }
          for (ic.outer, 0, 2) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 8) {
                  unrolled (ow.c.inner, 0, 30) {
                    conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*248712) + ((c.outer.h.fused % 64)*3856)) + (kh*1928)) + (ic.inner*241)) + (ow.outer*60)) + (ow.c.inner*2)) + kw)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.fused/64)*1152) + (ic.outer*576)) + (kh*192)) + (kw*64)) + (ic.inner*8)), 1, 8)])))
                  }
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
