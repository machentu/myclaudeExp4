// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 12288]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 4608]
produce data_vec {
  parallel (C.h.fused, 0, 192) {
    for (c, 0, 8) {
      for (w, 0, 8) {
        data_vec[(((C.h.fused*64) + (c*8)) + w)] = placeholder[(((((C.h.fused/16)*1024) + (c*128)) + ((C.h.fused % 16)*8)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 48) {
    for (CI, 0, 12) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*768) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*768) + (CI*8)) + ci), 96, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 384) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 16]
    produce conv {
      produce conv.global {
        conv.global[ramp(((0*64) + (0*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (1*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (2*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (3*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (4*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (5*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (6*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*64) + (7*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (0*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (1*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (2*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (3*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (4*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (5*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (6*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*64) + (7*8)), 1, 8)] = x8((int64)0)
        for (ic.outer, 0, 12) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(((0*64) + (0*8)), 1, 8)] = (conv.global[ramp(((0*64) + (0*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (1*8)), 1, 8)] = (conv.global[ramp(((0*64) + (1*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (2*8)), 1, 8)] = (conv.global[ramp(((0*64) + (2*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (3*8)), 1, 8)] = (conv.global[ramp(((0*64) + (3*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (4*8)), 1, 8)] = (conv.global[ramp(((0*64) + (4*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (5*8)), 1, 8)] = (conv.global[ramp(((0*64) + (5*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (6*8)), 1, 8)] = (conv.global[ramp(((0*64) + (6*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*64) + (7*8)), 1, 8)] = (conv.global[ramp(((0*64) + (7*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (0*64)) + (ic.inner*8)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (0*8)), 1, 8)] = (conv.global[ramp(((1*64) + (0*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (1*8)), 1, 8)] = (conv.global[ramp(((1*64) + (1*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (2*8)), 1, 8)] = (conv.global[ramp(((1*64) + (2*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (3*8)), 1, 8)] = (conv.global[ramp(((1*64) + (3*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (4*8)), 1, 8)] = (conv.global[ramp(((1*64) + (4*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (5*8)), 1, 8)] = (conv.global[ramp(((1*64) + (5*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (6*8)), 1, 8)] = (conv.global[ramp(((1*64) + (6*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*64) + (7*8)), 1, 8)] = (conv.global[ramp(((1*64) + (7*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*1024) + ((c.outer.h.outer.fused % 8)*128)) + (1*64)) + (ic.inner*8)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/8)*768) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
          }
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 8) {
          conv.global[ramp(((oh.inner*64) + (ow*8)), 1, 8)] = conv.global[ramp(((oh.inner*64) + (ow*8)), 1, 8)]
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 8) {
        output_unpack[ramp((((((c.outer.h.outer.fused/8)*1024) + ((c.outer.h.outer.fused % 8)*16)) + (h.inner*8)) + w.inner), 128, 8)] = conv.global[ramp(((h.inner*64) + (w.inner*8)), 1, 8)]
      }
    }
  }
}
