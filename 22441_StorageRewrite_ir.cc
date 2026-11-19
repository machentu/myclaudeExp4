// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 30720]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 512]
produce data_vec {
  parallel (C.h.fused, 0, 128) {
    for (c, 0, 8) {
      for (w, 0, 30) {
        data_vec[(((C.h.fused*240) + (c*30)) + w)] = placeholder[(((((C.h.fused/16)*3840) + (c*480)) + ((C.h.fused % 16)*30)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 8) {
    for (CI, 0, 8) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*512) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*512) + (CI*8)) + ci), 64, 8)]
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
        unrolled (ow.c.inner.init, 0, 30) {
          conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
        }
        for (ic.outer, 0, 8) {
          for (ic.inner, 0, 8) {
            unrolled (ow.c.inner, 0, 30) {
              conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*3840) + ((c.outer.h.outer.fused % 16)*240)) + (ic.inner*30)) + ow.c.inner)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            }
          }
        }
      }
      for (ow, 0, 30) {
        conv.global[ramp((ow*8), 1, 8)] = conv.global[ramp((ow*8), 1, 8)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.outer.fused/16)*3840) + ((c.outer.h.outer.fused % 16)*30)) + w.inner), 480, 8)] = conv.global[ramp((w.inner*8), 1, 8)]
    }
  }
}
