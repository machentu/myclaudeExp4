// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 29376]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 7803]
produce data_vec {
  parallel (C.h.fused, 0, 306) {
    for (c, 0, 3) {
      for (w, 0, 32) {
        data_vec[(((C.h.fused*96) + (c*32)) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 18)) && ((C.h.fused % 18) < 17)) && (1 <= w)) && (w < 31)), placeholder[((((((C.h.fused/18)*1440) + (c*480)) + ((C.h.fused % 18)*30)) + w) - 31)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 51) {
    for (CI, 0, 17) {
      for (w, 0, 3) {
        for (ci, 0, 3) {
          kernel_vec[ramp(((((((CO.h.fused/3)*1377) + (CI*81)) + ((CO.h.fused % 3)*27)) + (w*9)) + (ci*3)), 1, 3)] = placeholder[ramp(((((((CO.h.fused/3)*1377) + (CI*27)) + (ci*9)) + ((CO.h.fused % 3)*3)) + w), 459, 3)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 272) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 30]
    produce conv {
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 30) {
          conv.global[ramp((ow.c.inner.init*3), 1, 3)] = x3((int64)0)
        }
        for (ic.outer, 0, 17) {
          for (kh, 0, 3) {
            for (kw, 0, 3) {
              for (ic.inner, 0, 3) {
                unrolled (ow.c.inner, 0, 30) {
                  conv.global[ramp((ow.c.inner*3), 1, 3)] = (conv.global[ramp((ow.c.inner*3), 1, 3)] + (x3(int64(data_vec[((((((ic.outer*1728) + (kh*96)) + ((c.outer.h.fused % 16)*96)) + (ic.inner*32)) + ow.c.inner) + kw)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.fused/16)*1377) + (ic.outer*81)) + (kh*27)) + (kw*9)) + (ic.inner*3)), 1, 3)])))
                }
              }
            }
          }
        }
      }
      for (ow.inner, 0, 30) {
        conv.global[ramp((ow.inner*3), 1, 3)] = conv.global[ramp((ow.inner*3), 1, 3)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.fused/16)*1440) + ((c.outer.h.fused % 16)*30)) + w.inner), 480, 3)] = conv.global[ramp((w.inner*3), 1, 3)]
    }
  }
}
