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
        conv.global[0] = (int64)0
        conv.global[1] = (int64)0
        conv.global[2] = (int64)0
        conv.global[3] = (int64)0
        conv.global[4] = (int64)0
        conv.global[5] = (int64)0
        conv.global[6] = (int64)0
        conv.global[7] = (int64)0
        conv.global[8] = (int64)0
        conv.global[9] = (int64)0
        conv.global[10] = (int64)0
        conv.global[11] = (int64)0
        conv.global[12] = (int64)0
        conv.global[13] = (int64)0
        conv.global[14] = (int64)0
        conv.global[15] = (int64)0
        conv.global[16] = (int64)0
        conv.global[17] = (int64)0
        conv.global[18] = (int64)0
        conv.global[19] = (int64)0
        conv.global[20] = (int64)0
        conv.global[21] = (int64)0
        conv.global[22] = (int64)0
        conv.global[23] = (int64)0
        conv.global[24] = (int64)0
        conv.global[25] = (int64)0
        conv.global[26] = (int64)0
        conv.global[27] = (int64)0
        conv.global[28] = (int64)0
        conv.global[29] = (int64)0
        for (ic.outer, 0, 64) {
          conv.global[0] = (conv.global[0] + (int64(data_vec[(((ic.outer*4) + c.outer.h.outer.fused)*30)])*int64(kernel_vec[ic.outer])))
          conv.global[1] = (conv.global[1] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 1)])*int64(kernel_vec[ic.outer])))
          conv.global[2] = (conv.global[2] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 2)])*int64(kernel_vec[ic.outer])))
          conv.global[3] = (conv.global[3] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 3)])*int64(kernel_vec[ic.outer])))
          conv.global[4] = (conv.global[4] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 4)])*int64(kernel_vec[ic.outer])))
          conv.global[5] = (conv.global[5] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 5)])*int64(kernel_vec[ic.outer])))
          conv.global[6] = (conv.global[6] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 6)])*int64(kernel_vec[ic.outer])))
          conv.global[7] = (conv.global[7] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 7)])*int64(kernel_vec[ic.outer])))
          conv.global[8] = (conv.global[8] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 8)])*int64(kernel_vec[ic.outer])))
          conv.global[9] = (conv.global[9] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 9)])*int64(kernel_vec[ic.outer])))
          conv.global[10] = (conv.global[10] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 10)])*int64(kernel_vec[ic.outer])))
          conv.global[11] = (conv.global[11] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 11)])*int64(kernel_vec[ic.outer])))
          conv.global[12] = (conv.global[12] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 12)])*int64(kernel_vec[ic.outer])))
          conv.global[13] = (conv.global[13] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 13)])*int64(kernel_vec[ic.outer])))
          conv.global[14] = (conv.global[14] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 14)])*int64(kernel_vec[ic.outer])))
          conv.global[15] = (conv.global[15] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 15)])*int64(kernel_vec[ic.outer])))
          conv.global[16] = (conv.global[16] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 16)])*int64(kernel_vec[ic.outer])))
          conv.global[17] = (conv.global[17] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 17)])*int64(kernel_vec[ic.outer])))
          conv.global[18] = (conv.global[18] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 18)])*int64(kernel_vec[ic.outer])))
          conv.global[19] = (conv.global[19] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 19)])*int64(kernel_vec[ic.outer])))
          conv.global[20] = (conv.global[20] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 20)])*int64(kernel_vec[ic.outer])))
          conv.global[21] = (conv.global[21] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 21)])*int64(kernel_vec[ic.outer])))
          conv.global[22] = (conv.global[22] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 22)])*int64(kernel_vec[ic.outer])))
          conv.global[23] = (conv.global[23] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 23)])*int64(kernel_vec[ic.outer])))
          conv.global[24] = (conv.global[24] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 24)])*int64(kernel_vec[ic.outer])))
          conv.global[25] = (conv.global[25] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 25)])*int64(kernel_vec[ic.outer])))
          conv.global[26] = (conv.global[26] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 26)])*int64(kernel_vec[ic.outer])))
          conv.global[27] = (conv.global[27] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 27)])*int64(kernel_vec[ic.outer])))
          conv.global[28] = (conv.global[28] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 28)])*int64(kernel_vec[ic.outer])))
          conv.global[29] = (conv.global[29] + (int64(data_vec[((((ic.outer*4) + c.outer.h.outer.fused)*30) + 29)])*int64(kernel_vec[ic.outer])))
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[((((c.outer.h.outer.fused*2) + h.inner)*15) + w.inner)] = conv.global[((h.inner*15) + w.inner)]
      }
    }
  }
}
