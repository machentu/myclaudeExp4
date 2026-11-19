// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 7680]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 64]
produce data_vec {
  parallel (C.h.fused, 0, 512) {
    for (w, 0, 15) {
      data_vec[((C.h.fused*15) + w)] = placeholder[((C.h.fused*15) + w)]
    }
  }
}
produce kernel_vec {
  for (CI, 0, 64) {
    kernel_vec[CI] = placeholder[CI]
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 4) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 30]
    produce conv {
      produce conv.global {
        conv.global[((0*15) + 0)] = (int64)0
        conv.global[((0*15) + 1)] = (int64)0
        conv.global[((0*15) + 2)] = (int64)0
        conv.global[((0*15) + 3)] = (int64)0
        conv.global[((0*15) + 4)] = (int64)0
        conv.global[((0*15) + 5)] = (int64)0
        conv.global[((0*15) + 6)] = (int64)0
        conv.global[((0*15) + 7)] = (int64)0
        conv.global[((0*15) + 8)] = (int64)0
        conv.global[((0*15) + 9)] = (int64)0
        conv.global[((0*15) + 10)] = (int64)0
        conv.global[((0*15) + 11)] = (int64)0
        conv.global[((0*15) + 12)] = (int64)0
        conv.global[((0*15) + 13)] = (int64)0
        conv.global[((0*15) + 14)] = (int64)0
        conv.global[((1*15) + 0)] = (int64)0
        conv.global[((1*15) + 1)] = (int64)0
        conv.global[((1*15) + 2)] = (int64)0
        conv.global[((1*15) + 3)] = (int64)0
        conv.global[((1*15) + 4)] = (int64)0
        conv.global[((1*15) + 5)] = (int64)0
        conv.global[((1*15) + 6)] = (int64)0
        conv.global[((1*15) + 7)] = (int64)0
        conv.global[((1*15) + 8)] = (int64)0
        conv.global[((1*15) + 9)] = (int64)0
        conv.global[((1*15) + 10)] = (int64)0
        conv.global[((1*15) + 11)] = (int64)0
        conv.global[((1*15) + 12)] = (int64)0
        conv.global[((1*15) + 13)] = (int64)0
        conv.global[((1*15) + 14)] = (int64)0
        for (ic.outer, 0, 64) {
          conv.global[((0*15) + 0)] = (conv.global[((0*15) + 0)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 0)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 1)] = (conv.global[((0*15) + 1)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 1)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 2)] = (conv.global[((0*15) + 2)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 2)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 3)] = (conv.global[((0*15) + 3)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 3)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 4)] = (conv.global[((0*15) + 4)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 4)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 5)] = (conv.global[((0*15) + 5)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 5)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 6)] = (conv.global[((0*15) + 6)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 6)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 7)] = (conv.global[((0*15) + 7)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 7)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 8)] = (conv.global[((0*15) + 8)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 8)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 9)] = (conv.global[((0*15) + 9)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 9)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 10)] = (conv.global[((0*15) + 10)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 10)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 11)] = (conv.global[((0*15) + 11)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 11)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 12)] = (conv.global[((0*15) + 12)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 12)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 13)] = (conv.global[((0*15) + 13)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 13)])*int64(kernel_vec[ic.outer])))
          conv.global[((0*15) + 14)] = (conv.global[((0*15) + 14)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (0*15)) + 14)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 0)] = (conv.global[((1*15) + 0)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 0)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 1)] = (conv.global[((1*15) + 1)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 1)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 2)] = (conv.global[((1*15) + 2)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 2)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 3)] = (conv.global[((1*15) + 3)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 3)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 4)] = (conv.global[((1*15) + 4)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 4)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 5)] = (conv.global[((1*15) + 5)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 5)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 6)] = (conv.global[((1*15) + 6)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 6)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 7)] = (conv.global[((1*15) + 7)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 7)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 8)] = (conv.global[((1*15) + 8)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 8)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 9)] = (conv.global[((1*15) + 9)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 9)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 10)] = (conv.global[((1*15) + 10)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 10)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 11)] = (conv.global[((1*15) + 11)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 11)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 12)] = (conv.global[((1*15) + 12)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 12)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 13)] = (conv.global[((1*15) + 13)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 13)])*int64(kernel_vec[ic.outer])))
          conv.global[((1*15) + 14)] = (conv.global[((1*15) + 14)] + (int64(data_vec[((((ic.outer*120) + (c.outer.h.outer.fused*30)) + (1*15)) + 14)])*int64(kernel_vec[ic.outer])))
        }
      }
      for (oh.inner, 0, 2) {
        for (ow, 0, 15) {
          conv.global[((oh.inner*15) + ow)] = conv.global[((oh.inner*15) + ow)]
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[(((c.outer.h.outer.fused*30) + (h.inner*15)) + w.inner)] = conv.global[((h.inner*15) + w.inner)]
      }
    }
  }
}
