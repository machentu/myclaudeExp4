// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 8 * 32 * 8 * 16]
produce data_vec {
  parallel (C.h.fused, 0, 256) {
    for (c, 0, 8) {
      for (w, 0, 16) {
        data_vec[(((C.h.fused*128) + (c*16)) + w)] = placeholder[(((((C.h.fused/32)*4096) + (c*512)) + ((C.h.fused % 32)*16)) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 32 * 8 * 1 * 1 * 8 * 8]
produce kernel_vec {
  parallel (CO.h.fused, 0, 32) {
    for (CI, 0, 8) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*512) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*512) + (CI*8)) + ci), 64, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 1024) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 16 * 8]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 16 * 8]
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 16) {
          conv.global[ramp((ow.c.inner.init*8), 1, 8)] = x8((int64)0)
        }
        for (ic.outer, 0, 8) {
          for (ic.inner, 0, 8) {
            unrolled (ow.c.inner, 0, 16) {
              conv.global[ramp((ow.c.inner*8), 1, 8)] = (conv.global[ramp((ow.c.inner*8), 1, 8)] + (x8(int64(data_vec[((((ic.outer*4096) + ((c.outer.h.outer.fused % 32)*128)) + (ic.inner*16)) + ow.c.inner)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*512) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            }
          }
        }
      }
      for (ow, 0, 16) {
        conv[ramp((ow*8), 1, 8)] = conv.global[ramp((ow*8), 1, 8)]
      }
    }
    for (w.inner, 0, 16) {
      output_unpack[ramp(((((c.outer.h.outer.fused/32)*4096) + ((c.outer.h.outer.fused % 32)*16)) + w.inner), 512, 8)] = conv[ramp((w.inner*8), 1, 8)]
    }
  }
}
