// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 30720]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 8192]
produce data_vec {
  parallel (C.h.fused, 0, 256) {
    for (c, 0, 8) {
      for (w, 0, 15) {
        data_vec[(((C.h.fused*120) + (c*15)) + w)] = placeholder[(((((C.h.fused/8)*960) + (c*120)) + ((C.h.fused % 8)*15)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 32) {
    for (CI, 0, 32) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*2048) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*2048) + (CI*8)) + ci), 256, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 128) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      produce conv.global {
        unrolled (oh.c.inner.init, 0, 2) {
          unrolled (ow.c.inner.init, 0, 15) {
            conv.global[ramp(((oh.c.inner.init*120) + (ow.c.inner.init*8)), 1, 8)] = x8((int64)0)
          }
        }
        for (ic.outer, 0, 32) {
          for (ic.inner, 0, 8) {
            unrolled (oh.c.inner, 0, 2) {
              unrolled (ow.c.inner, 0, 15) {
                conv.global[ramp(((oh.c.inner*120) + (ow.c.inner*8)), 1, 8)] = (conv.global[ramp(((oh.c.inner*120) + (ow.c.inner*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (oh.c.inner*120)) + (ic.inner*15)) + ow.c.inner)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*2048) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              }
            }
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 15) {
          conv.global[ramp(((oh.inner*120) + (ow*8)), 1, 8)] = conv.global[ramp(((oh.inner*120) + (ow*8)), 1, 8)]
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[ramp((((((c.outer.h.outer.fused/4)*960) + ((c.outer.h.outer.fused % 4)*30)) + (h.inner*15)) + w.inner), 120, 8)] = conv.global[ramp(((h.inner*120) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
