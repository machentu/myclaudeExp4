// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 245760]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 1024]
produce data_vec {
  parallel (C.h.fused, 0, 512) {
    for (c, 0, 8) {
      for (w, 0, 60) {
        data_vec[(((C.h.fused*480) + (c*60)) + w)] = placeholder[(((((C.h.fused/32)*15360) + (c*1920)) + ((C.h.fused % 32)*60)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 8) {
    for (CI, 0, 16) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*1024) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*1024) + (CI*8)) + ci), 128, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 60]
    produce conv {
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[ramp(((ow.c.outer*240) + (ow.c.inner.init*8)), 1, 8)] = x8((int64)0)
          }
          for (ic.outer, 0, 16) {
            for (ic.inner, 0, 8) {
              unrolled (ow.c.inner, 0, 30) {
                conv.global[ramp(((ow.c.outer*240) + (ow.c.inner*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (ow.c.inner*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + ow.c.inner)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*1024) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              }
            }
          }
        }
      }
      for (ow, 0, 60) {
        conv.global[ramp((ow*8), 1, 8)] = conv.global[ramp((ow*8), 1, 8)]
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.outer.fused/32)*15360) + ((c.outer.h.outer.fused % 32)*60)) + (w.outer*30)) + w.inner), 1920, 8)] = conv.global[ramp(((w.outer*240) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
