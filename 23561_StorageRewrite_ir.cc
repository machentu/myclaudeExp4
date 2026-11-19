// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 6120]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 136) {
    for (c, 0, 3) {
      for (w, 0, 15) {
        data_vec[(((C.h.fused*45) + (c*15)) + w)] = placeholder[(((((C.h.fused/8)*360) + (c*120)) + ((C.h.fused % 8)*15)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((CO.h.fused*153) + (CI*9)) + (ci*3)), 1, 3)] = placeholder[ramp((((CO.h.fused*153) + (CI*3)) + ci), 51, 3)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 68) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 30]
    produce conv {
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 2) {
          unrolled (ow.c.inner.init, 0, 15) {
            conv.global[ramp(((oh.c.inner.init*45) + (ow.c.inner.init*3)), 1, 3)] = x3((int64)0)
          }
        }
        for (ic.outer, 0, 17) {
          for (ic.inner, 0, 3) {
            unrolled (oh.c.inner, 0, 2) {
              unrolled (ow.c.inner, 0, 15) {
                conv.global[ramp(((oh.c.inner*45) + (ow.c.inner*3)), 1, 3)] = (conv.global[ramp(((oh.c.inner*45) + (ow.c.inner*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (oh.c.inner*45)) + (ic.inner*15)) + ow.c.inner)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              }
            }
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 15) {
          conv.global[ramp(((oh.inner*45) + (ow*3)), 1, 3)] = conv.global[ramp(((oh.inner*45) + (ow*3)), 1, 3)]
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[ramp((((((c.outer.h.outer.fused/4)*360) + ((c.outer.h.outer.fused % 4)*30)) + (h.inner*15)) + w.inner), 120, 3)] = conv.global[ramp(((h.inner*45) + (w.inner*3)), 1, 3)]
      }
    }
  }
}
