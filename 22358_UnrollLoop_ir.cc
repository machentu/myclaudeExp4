// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 122880]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 64]
produce data_vec {
  parallel (C.h.fused, 0, 2048) {
    for (w, 0, 60) {
      data_vec[((C.h.fused*60) + w)] = placeholder[((C.h.fused*60) + w)]
    }
  }
}
produce kernel_vec {
  for (CI, 0, 64) {
    kernel_vec[CI] = placeholder[CI]
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 32) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 60]
    produce conv {
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          conv.global[((ow.c.outer*30) + 0)] = (int64)0
          conv.global[((ow.c.outer*30) + 1)] = (int64)0
          conv.global[((ow.c.outer*30) + 2)] = (int64)0
          conv.global[((ow.c.outer*30) + 3)] = (int64)0
          conv.global[((ow.c.outer*30) + 4)] = (int64)0
          conv.global[((ow.c.outer*30) + 5)] = (int64)0
          conv.global[((ow.c.outer*30) + 6)] = (int64)0
          conv.global[((ow.c.outer*30) + 7)] = (int64)0
          conv.global[((ow.c.outer*30) + 8)] = (int64)0
          conv.global[((ow.c.outer*30) + 9)] = (int64)0
          conv.global[((ow.c.outer*30) + 10)] = (int64)0
          conv.global[((ow.c.outer*30) + 11)] = (int64)0
          conv.global[((ow.c.outer*30) + 12)] = (int64)0
          conv.global[((ow.c.outer*30) + 13)] = (int64)0
          conv.global[((ow.c.outer*30) + 14)] = (int64)0
          conv.global[((ow.c.outer*30) + 15)] = (int64)0
          conv.global[((ow.c.outer*30) + 16)] = (int64)0
          conv.global[((ow.c.outer*30) + 17)] = (int64)0
          conv.global[((ow.c.outer*30) + 18)] = (int64)0
          conv.global[((ow.c.outer*30) + 19)] = (int64)0
          conv.global[((ow.c.outer*30) + 20)] = (int64)0
          conv.global[((ow.c.outer*30) + 21)] = (int64)0
          conv.global[((ow.c.outer*30) + 22)] = (int64)0
          conv.global[((ow.c.outer*30) + 23)] = (int64)0
          conv.global[((ow.c.outer*30) + 24)] = (int64)0
          conv.global[((ow.c.outer*30) + 25)] = (int64)0
          conv.global[((ow.c.outer*30) + 26)] = (int64)0
          conv.global[((ow.c.outer*30) + 27)] = (int64)0
          conv.global[((ow.c.outer*30) + 28)] = (int64)0
          conv.global[((ow.c.outer*30) + 29)] = (int64)0
          for (ic.outer, 0, 64) {
            conv.global[((ow.c.outer*30) + 0)] = (conv.global[((ow.c.outer*30) + 0)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 0)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 1)] = (conv.global[((ow.c.outer*30) + 1)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 1)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 2)] = (conv.global[((ow.c.outer*30) + 2)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 2)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 3)] = (conv.global[((ow.c.outer*30) + 3)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 3)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 4)] = (conv.global[((ow.c.outer*30) + 4)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 4)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 5)] = (conv.global[((ow.c.outer*30) + 5)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 5)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 6)] = (conv.global[((ow.c.outer*30) + 6)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 6)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 7)] = (conv.global[((ow.c.outer*30) + 7)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 7)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 8)] = (conv.global[((ow.c.outer*30) + 8)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 8)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 9)] = (conv.global[((ow.c.outer*30) + 9)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 9)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 10)] = (conv.global[((ow.c.outer*30) + 10)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 10)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 11)] = (conv.global[((ow.c.outer*30) + 11)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 11)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 12)] = (conv.global[((ow.c.outer*30) + 12)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 12)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 13)] = (conv.global[((ow.c.outer*30) + 13)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 13)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 14)] = (conv.global[((ow.c.outer*30) + 14)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 14)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 15)] = (conv.global[((ow.c.outer*30) + 15)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 15)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 16)] = (conv.global[((ow.c.outer*30) + 16)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 16)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 17)] = (conv.global[((ow.c.outer*30) + 17)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 17)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 18)] = (conv.global[((ow.c.outer*30) + 18)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 18)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 19)] = (conv.global[((ow.c.outer*30) + 19)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 19)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 20)] = (conv.global[((ow.c.outer*30) + 20)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 20)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 21)] = (conv.global[((ow.c.outer*30) + 21)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 21)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 22)] = (conv.global[((ow.c.outer*30) + 22)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 22)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 23)] = (conv.global[((ow.c.outer*30) + 23)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 23)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 24)] = (conv.global[((ow.c.outer*30) + 24)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 24)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 25)] = (conv.global[((ow.c.outer*30) + 25)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 25)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 26)] = (conv.global[((ow.c.outer*30) + 26)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 26)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 27)] = (conv.global[((ow.c.outer*30) + 27)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 27)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 28)] = (conv.global[((ow.c.outer*30) + 28)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 28)])*int64(kernel_vec[ic.outer])))
            conv.global[((ow.c.outer*30) + 29)] = (conv.global[((ow.c.outer*30) + 29)] + (int64(data_vec[((((ic.outer*1920) + (c.outer.h.outer.fused*60)) + (ow.c.outer*30)) + 29)])*int64(kernel_vec[ic.outer])))
          }
        }
      }
      for (ow, 0, 60) {
        conv.global[ow] = conv.global[ow]
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[(((c.outer.h.outer.fused*60) + (w.outer*30)) + w.inner)] = conv.global[((w.outer*30) + w.inner)]
      }
    }
  }
}
