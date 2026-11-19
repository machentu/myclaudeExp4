// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 32 * 34 * 2 * 62]
produce data_vec {
  parallel (C.h.fused, 0, 1088) {
    for (c, 0, 2) {
      for (w, 0, 62) {
        data_vec[(((C.h.fused*124) + (c*62)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((C.h.fused/34)*3840) + (c*1920)) + ((C.h.fused % 34)*60)) + w) - 61)], (int8)0)
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 32 * 3 * 3 * 2 * 3]
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 32) {
      for (w, 0, 3) {
        for (ci, 0, 2) {
          kernel_vec[ramp(((((((CO.h.fused/3)*1728) + (CI*54)) + ((CO.h.fused % 3)*18)) + (w*6)) + (ci*3)), 1, 3)] = placeholder[ramp(((((((CO.h.fused/3)*1728) + (CI*18)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 576, 3)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 544) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 3]
    produce conv {
      for (ow.outer, 0, 2) {
        // attr [conv.global] storage_scope = "global"
        allocate conv.global[int64 * 1 * 1 * 1 * 30 * 3]
        produce conv.global {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[ramp((ow.c.inner.init*3), 1, 3)] = x3((int64)0)
          }
          for (ic.outer, 0, 32) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 2) {
                  unrolled (ow.c.inner, 0, 30) {
                    conv.global[ramp((ow.c.inner*3), 1, 3)] = (conv.global[ramp((ow.c.inner*3), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*4216) + (kh*124)) + ((c.outer.h.fused % 32)*124)) + (ic.inner*62)) + (ow.outer*30)) + ow.c.inner) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/32)*1728) + (ic.outer*54)) + (kh*18)) + (kw*6)) + (ic.inner*3)), 1, 3)])))
                  }
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp(((ow.outer*90) + (ow.inner*3)), 1, 3)] = conv.global[ramp((ow.inner*3), 1, 3)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.fused/32)*5760) + ((c.outer.h.fused % 32)*60)) + (w.outer*30)) + w.inner), 1920, 3)] = conv[ramp(((w.outer*90) + (w.inner*3)), 1, 3)]
      }
    }
  }
}
