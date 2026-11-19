// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 24480]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 3) {
      for (w, 0, 30) {
        data_vec[(((C.h.fused*90) + (c*30)) + w)] = placeholder[(((((C.h.fused/16)*1440) + (c*480)) + ((C.h.fused % 16)*30)) + w)]
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
  parallel (c.outer.h.outer.fused, 0, 272) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 30]
    produce conv {
      produce conv.global {
        unrolled (ow.c.inner.init, 0, 30) {
          conv.global[ramp((ow.c.inner.init*3), 1, 3)] = x3((int64)0)
        }
        for (ic.outer, 0, 17) {
          for (ic.inner, 0, 3) {
            unrolled (ow.c.inner, 0, 30) {
              conv.global[ramp((ow.c.inner*3), 1, 3)] = (conv.global[ramp((ow.c.inner*3), 1, 3)] + (x3(int64(data_vec[((((ic.outer*1440) + ((c.outer.h.outer.fused % 16)*90)) + (ic.inner*30)) + ow.c.inner)]))*int64x3(kernel_vec[ramp(((((c.outer.h.outer.fused/16)*153) + (ic.outer*9)) + (ic.inner*3)), 1, 3)])))
            }
          }
        }
      }
      for (ow, 0, 30) {
        conv.global[ramp((ow*3), 1, 3)] = conv.global[ramp((ow*3), 1, 3)]
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[ramp(((((c.outer.h.outer.fused/16)*1440) + ((c.outer.h.outer.fused % 16)*30)) + w.inner), 480, 3)] = conv.global[ramp((w.inner*3), 1, 3)]
    }
  }
}
