// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 61440]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 128]
produce data_vec {
  parallel (C.h.fused, 0, 128) {
    for (c, 0, 8) {
      for (w, 0, 60) {
        data_vec[(((C.h.fused*480) + (c*60)) + w)] = placeholder[(((((C.h.fused/32)*15360) + (c*1920)) + ((C.h.fused % 32)*60)) + w)]
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
  parallel (c.outer.h.outer.fused, 0, 128) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 60]
    produce conv {
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          conv.global[ramp(((ow.c.outer*240) + (0*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (1*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (2*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (3*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (4*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (5*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (6*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (7*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (8*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (9*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (10*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (11*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (12*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (13*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (14*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (15*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (16*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (17*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (18*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (19*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (20*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (21*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (22*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (23*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (24*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (25*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (26*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (27*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (28*8)), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + (29*8)), 1, 8)] = x8((int64)0)
          for (ic.outer, 0, 4) {
            for (ic.inner, 0, 8) {
              conv.global[ramp(((ow.c.outer*240) + (0*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (0*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 0)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (1*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (1*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 1)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (2*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (2*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 2)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (3*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (3*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 3)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (4*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (4*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 4)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (5*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (5*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 5)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (6*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (6*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 6)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (7*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (7*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 7)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (8*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (8*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 8)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (9*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (9*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 9)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (10*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (10*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 10)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (11*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (11*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 11)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (12*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (12*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 12)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (13*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (13*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 13)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (14*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (14*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 14)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (15*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (15*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 15)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (16*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (16*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 16)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (17*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (17*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 17)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (18*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (18*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 18)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (19*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (19*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 19)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (20*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (20*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 20)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (21*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (21*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 21)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (22*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (22*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 22)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (23*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (23*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 23)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (24*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (24*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 24)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (25*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (25*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 25)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (26*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (26*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 26)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (27*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (27*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 27)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (28*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (28*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 28)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + (29*8)), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + (29*8)), 1, 8)] + (x8(int64(data_vec[(((((ic.outer*15360) + ((c.outer.h.outer.fused % 32)*480)) + (ic.inner*60)) + (ow.c.outer*30)) + 29)]))*int64x8(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*256) + (ic.outer*64)) + (ic.inner*8)), 1, 8)])))
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
