// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 245760]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 1024]
produce data_vec {
  parallel (C.h.fused, 0, 512) {
    for (c, 0, 8) {
      for (w, 0, 60) {
        data_vec[((((C.h.fused*8) + c)*60) + w)] = placeholder[(((((((C.h.fused/32)*8) + c)*32) + (C.h.fused % 32))*60) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 8) {
    for (CI, 0, 16) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*16) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*128) + CI)*8) + ci), 128, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 256) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 60]
    produce conv {
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          conv.global[ramp((ow.c.outer*240), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 8), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 16), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 24), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 32), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 40), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 48), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 56), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 64), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 72), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 80), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 88), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 96), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 104), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 112), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 120), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 128), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 136), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 144), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 152), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 160), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 168), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 176), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 184), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 192), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 200), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 208), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 216), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 224), 1, 8)] = x8((int64)0)
          conv.global[ramp(((ow.c.outer*240) + 232), 1, 8)] = x8((int64)0)
          for (ic.outer, 0, 16) {
            for (ic.inner, 0, 8) {
              conv.global[ramp((ow.c.outer*240), 1, 8)] = (conv.global[ramp((ow.c.outer*240), 1, 8)] + (x8(int64(data_vec[(((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 8), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 8), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 16), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 16), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 24), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 24), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 32), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 32), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 40), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 40), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 5)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 48), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 48), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 6)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 56), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 56), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 7)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 64), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 64), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 8)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 72), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 72), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 9)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 80), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 80), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 10)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 88), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 88), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 11)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 96), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 96), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 12)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 104), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 104), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 13)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 112), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 112), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 14)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 120), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 120), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 15)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 128), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 128), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 16)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 136), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 136), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 17)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 144), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 144), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 18)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 152), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 152), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 19)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 160), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 160), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 20)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 168), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 168), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 21)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 176), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 176), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 22)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 184), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 184), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 23)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 192), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 192), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 24)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 200), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 200), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 25)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 208), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 208), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 26)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 216), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 216), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 27)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 224), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 224), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 28)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
              conv.global[ramp(((ow.c.outer*240) + 232), 1, 8)] = (conv.global[ramp(((ow.c.outer*240) + 232), 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*8) + ic.inner)*2) + ow.c.outer)*30) + 29)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*16) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            }
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/32)*256) + (c.outer.h.outer.fused % 32))*2) + w.outer)*30) + w.inner), 1920, 8)] = conv.global[ramp((((w.outer*30) + w.inner)*8), 1, 8)]
      }
    }
  }
}
