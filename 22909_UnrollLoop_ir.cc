// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 3840]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 128]
produce data_vec {
  parallel (C.h.fused, 0, 32) {
    for (c, 0, 8) {
      for (w, 0, 15) {
        data_vec[(((C.h.fused*120) + (c*15)) + w)] = placeholder[(((((C.h.fused/8)*960) + (c*120)) + ((C.h.fused % 8)*15)) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 4) {
    for (CI, 0, 4) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((CO.h.fused*256) + (CI*64)) + (ci*8)), 1, 8)] = placeholder[ramp((((CO.h.fused*256) + (CI*8)) + ci), 32, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 16) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      produce conv.global {
        conv.global[ramp(((0*120) + (0*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (1*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (2*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (3*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (4*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (5*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (6*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (7*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (8*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (9*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (10*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (11*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (12*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (13*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((0*120) + (14*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (0*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (1*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (2*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (3*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (4*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (5*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (6*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (7*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (8*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (9*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (10*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (11*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (12*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (13*8)), 1, 8)] = x8((int64)0)
        conv.global[ramp(((1*120) + (14*8)), 1, 8)] = x8((int64)0)
        for (ic.outer, 0, 4) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(((0*120) + (0*8)), 1, 8)] = (conv.global[ramp(((0*120) + (0*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (1*8)), 1, 8)] = (conv.global[ramp(((0*120) + (1*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (2*8)), 1, 8)] = (conv.global[ramp(((0*120) + (2*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (3*8)), 1, 8)] = (conv.global[ramp(((0*120) + (3*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (4*8)), 1, 8)] = (conv.global[ramp(((0*120) + (4*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (5*8)), 1, 8)] = (conv.global[ramp(((0*120) + (5*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (6*8)), 1, 8)] = (conv.global[ramp(((0*120) + (6*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (7*8)), 1, 8)] = (conv.global[ramp(((0*120) + (7*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (8*8)), 1, 8)] = (conv.global[ramp(((0*120) + (8*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 8)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (9*8)), 1, 8)] = (conv.global[ramp(((0*120) + (9*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 9)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (10*8)), 1, 8)] = (conv.global[ramp(((0*120) + (10*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 10)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (11*8)), 1, 8)] = (conv.global[ramp(((0*120) + (11*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 11)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (12*8)), 1, 8)] = (conv.global[ramp(((0*120) + (12*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 12)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (13*8)), 1, 8)] = (conv.global[ramp(((0*120) + (13*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 13)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((0*120) + (14*8)), 1, 8)] = (conv.global[ramp(((0*120) + (14*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (0*120)) + (ic.inner*15)) + 14)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (0*8)), 1, 8)] = (conv.global[ramp(((1*120) + (0*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (1*8)), 1, 8)] = (conv.global[ramp(((1*120) + (1*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (2*8)), 1, 8)] = (conv.global[ramp(((1*120) + (2*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (3*8)), 1, 8)] = (conv.global[ramp(((1*120) + (3*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (4*8)), 1, 8)] = (conv.global[ramp(((1*120) + (4*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (5*8)), 1, 8)] = (conv.global[ramp(((1*120) + (5*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (6*8)), 1, 8)] = (conv.global[ramp(((1*120) + (6*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (7*8)), 1, 8)] = (conv.global[ramp(((1*120) + (7*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (8*8)), 1, 8)] = (conv.global[ramp(((1*120) + (8*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 8)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (9*8)), 1, 8)] = (conv.global[ramp(((1*120) + (9*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 9)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (10*8)), 1, 8)] = (conv.global[ramp(((1*120) + (10*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 10)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (11*8)), 1, 8)] = (conv.global[ramp(((1*120) + (11*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 11)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (12*8)), 1, 8)] = (conv.global[ramp(((1*120) + (12*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 12)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (13*8)), 1, 8)] = (conv.global[ramp(((1*120) + (13*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 13)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
            conv.global[ramp(((1*120) + (14*8)), 1, 8)] = (conv.global[ramp(((1*120) + (14*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*960) + ((c.outer.h.outer.fused % 4)*240)) + (1*120)) + (ic.inner*15)) + 14)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
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
