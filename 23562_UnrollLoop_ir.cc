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
        conv.global[ramp(((0*45) + (0*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (1*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (2*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (3*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (4*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (5*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (6*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (7*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (8*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (9*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (10*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (11*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (12*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (13*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((0*45) + (14*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (0*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (1*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (2*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (3*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (4*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (5*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (6*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (7*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (8*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (9*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (10*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (11*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (12*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (13*3)), 1, 3)] = x3((int64)0)
        conv.global[ramp(((1*45) + (14*3)), 1, 3)] = x3((int64)0)
        for (ic.outer, 0, 17) {
          for (ic.inner, 0, 3) {
            conv.global[ramp(((0*45) + (0*3)), 1, 3)] = (conv.global[ramp(((0*45) + (0*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 0)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (1*3)), 1, 3)] = (conv.global[ramp(((0*45) + (1*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 1)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (2*3)), 1, 3)] = (conv.global[ramp(((0*45) + (2*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 2)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (3*3)), 1, 3)] = (conv.global[ramp(((0*45) + (3*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 3)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (4*3)), 1, 3)] = (conv.global[ramp(((0*45) + (4*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 4)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (5*3)), 1, 3)] = (conv.global[ramp(((0*45) + (5*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 5)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (6*3)), 1, 3)] = (conv.global[ramp(((0*45) + (6*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 6)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (7*3)), 1, 3)] = (conv.global[ramp(((0*45) + (7*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 7)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (8*3)), 1, 3)] = (conv.global[ramp(((0*45) + (8*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 8)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (9*3)), 1, 3)] = (conv.global[ramp(((0*45) + (9*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 9)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (10*3)), 1, 3)] = (conv.global[ramp(((0*45) + (10*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 10)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (11*3)), 1, 3)] = (conv.global[ramp(((0*45) + (11*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 11)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (12*3)), 1, 3)] = (conv.global[ramp(((0*45) + (12*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 12)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (13*3)), 1, 3)] = (conv.global[ramp(((0*45) + (13*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 13)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((0*45) + (14*3)), 1, 3)] = (conv.global[ramp(((0*45) + (14*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (0*45)) + (ic.inner*15)) + 14)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (0*3)), 1, 3)] = (conv.global[ramp(((1*45) + (0*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 0)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (1*3)), 1, 3)] = (conv.global[ramp(((1*45) + (1*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 1)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (2*3)), 1, 3)] = (conv.global[ramp(((1*45) + (2*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 2)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (3*3)), 1, 3)] = (conv.global[ramp(((1*45) + (3*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 3)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (4*3)), 1, 3)] = (conv.global[ramp(((1*45) + (4*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 4)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (5*3)), 1, 3)] = (conv.global[ramp(((1*45) + (5*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 5)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (6*3)), 1, 3)] = (conv.global[ramp(((1*45) + (6*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 6)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (7*3)), 1, 3)] = (conv.global[ramp(((1*45) + (7*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 7)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (8*3)), 1, 3)] = (conv.global[ramp(((1*45) + (8*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 8)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (9*3)), 1, 3)] = (conv.global[ramp(((1*45) + (9*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 9)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (10*3)), 1, 3)] = (conv.global[ramp(((1*45) + (10*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 10)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (11*3)), 1, 3)] = (conv.global[ramp(((1*45) + (11*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 11)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (12*3)), 1, 3)] = (conv.global[ramp(((1*45) + (12*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 12)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (13*3)), 1, 3)] = (conv.global[ramp(((1*45) + (13*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 13)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            conv.global[ramp(((1*45) + (14*3)), 1, 3)] = (conv.global[ramp(((1*45) + (14*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*360) + ((c.outer.h.outer.fused % 4)*90)) + (1*45)) + (ic.inner*15)) + 14)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/4)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
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
