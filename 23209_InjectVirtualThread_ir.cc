// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 1 * 17 * 32 * 3 * 60]
produce data_vec {
  parallel (C.h.fused, 0, 544) {
    for (c, 0, 3) {
      for (w, 0, 60) {
        data_vec[(((C.h.fused*180) + (c*60)) + w)] = placeholder[(((((C.h.fused/32)*5760) + (c*1920)) + ((C.h.fused % 32)*60)) + w)]
      }
    }
  }
}
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 17 * 17 * 1 * 1 * 3 * 3]
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
    // attr [conv] storage_scope = "global"
    allocate conv[int64 * 1 * 1 * 1 * 60 * 3]
    produce conv {
      // attr [conv.global] storage_scope = "global"
      allocate conv.global[int64 * 1 * 1 * 1 * 60 * 3]
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          unrolled (ow.c.inner.init, 0, 30) {
            conv.global[ramp(((ow.c.outer*90) + (ow.c.inner.init*3)), 1, 3)] = x3((int64)0)
          }
          for (ic.outer, 0, 17) {
            for (ic.inner, 0, 3) {
              unrolled (ow.c.inner, 0, 30) {
                conv.global[ramp(((ow.c.outer*90) + (ow.c.inner*3)), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + (ow.c.inner*3)), 1, 3)] + (x3(int64(data_vec[(((((ic.outer*5760) + ((c.outer.h.outer.fused % 32)*180)) + (ic.inner*60)) + (ow.c.outer*30)) + ow.c.inner)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/32)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
              }
            }
          }
        }
      }
      for (ow, 0, 60) {
        conv[ramp((ow*3), 1, 3)] = conv.global[ramp((ow*3), 1, 3)]
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((c.outer.h.outer.fused/32)*5760) + ((c.outer.h.outer.fused % 32)*60)) + (w.outer*30)) + w.inner), 1920, 3)] = conv[ramp(((w.outer*90) + (w.inner*3)), 1, 3)]
      }
    }
  }
}
