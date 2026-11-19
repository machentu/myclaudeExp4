// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 97920]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 544) {
    for (c, 0, 3) {
      for (w, 0, 60) {
        data_vec[(((C.h.fused*180) + (c*60)) + w)] = placeholder[(((((C.h.fused/32)*5760) + (c*1920)) + ((C.h.fused % 32)*60)) + w)]
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
  parallel (c.outer.h.outer.fused, 0, 544) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 60]
    produce conv {
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          conv.global[ramp(((ow.c.outer*90) + (0*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (1*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (2*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (3*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (4*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (5*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (6*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (7*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (8*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (9*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (10*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (11*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (12*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (13*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (14*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (15*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (16*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (17*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (18*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (19*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (20*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (21*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (22*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (23*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (24*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (25*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (26*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (27*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (28*3)), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + (29*3)), 1, 3)] = x3((int64)0)
          for (ic.outer, 0, 17) {
            for (ic.inner, 0, 3) {
              conv.global[ramp(((ow.c.outer*90) + (0*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (0*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 0)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (1*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (1*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 1)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (2*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (2*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 2)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (3*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (3*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 3)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (4*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (4*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 4)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (5*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (5*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 5)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (6*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (6*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 6)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (7*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (7*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 7)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (8*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (8*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 8)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (9*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (9*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 9)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (10*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (10*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 10)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (11*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (11*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 11)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (12*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (12*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 12)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (13*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (13*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 13)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (14*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (14*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 14)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (15*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (15*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 15)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (16*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (16*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 16)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (17*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (17*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 17)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (18*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (18*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 18)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (19*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (19*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 19)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (20*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (20*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 20)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (21*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (21*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 21)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (22*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (22*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 22)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (23*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (23*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 23)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (24*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (24*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 24)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (25*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (25*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 25)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (26*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (26*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 26)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (27*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (27*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 27)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (28*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (28*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 28)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + (29*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (29*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + 29)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            }
          }
        }
      }
      for (ow, 0, 60) {
        conv.global[ramp((ow*3), 1, 3)] = conv.global[ramp((ow*3), 1, 3)]
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.outer.fused/32)*5760) + ((c.outer.h.outer.fused % 32)*60)) + (w.outer*30)) + w.inner), 1920, 3)] = conv.global[ramp(((w.outer*90) + (w.inner*3)), 1, 3)]
      }
    }
  }
}
