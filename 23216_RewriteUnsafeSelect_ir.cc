// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 97920]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 544) {
    for (c, 0, 3) {
      for (w, 0, 60) {
        data_vec[((((C.h.fused*3) + c)*60) + w)] = placeholder[(((((((C.h.fused/32)*3) + c)*32) + (C.h.fused % 32))*60) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((((CO.h.fused*17) + CI)*3) + ci)*3), 1, 3)] = placeholder[ramp(((((CO.h.fused*51) + CI)*3) + ci), 51, 3)]
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
          conv.global[ramp((ow.c.outer*90), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 3), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 6), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 9), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 12), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 15), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 18), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 21), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 24), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 27), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 30), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 33), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 36), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 39), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 42), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 45), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 48), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 51), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 54), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 57), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 60), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 63), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 66), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 69), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 72), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 75), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 78), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 81), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 84), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 87), 1, 3)] = x3((int64)0)
          for (ic.outer, 0, 17) {
            for (ic.inner, 0, 3) {
              conv.global[ramp((ow.c.outer*90), 1, 3)] = (conv.global[ramp((ow.c.outer*90), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 3), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 3), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 1)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 6), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 6), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 2)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 9), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 9), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 3)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 12), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 12), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 4)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 15), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 15), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 5)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 18), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 18), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 6)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 21), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 21), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 7)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 24), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 24), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 8)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 27), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 27), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 9)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 30), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 30), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 10)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 33), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 33), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 11)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 36), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 36), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 12)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 39), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 39), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 13)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 42), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 42), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 14)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 45), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 45), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 15)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 48), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 48), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 16)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 51), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 51), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 17)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 54), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 54), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 18)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 57), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 57), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 19)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 60), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 60), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 20)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 63), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 63), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 21)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 66), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 66), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 22)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 69), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 69), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 23)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 72), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 72), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 24)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 75), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 75), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 25)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 78), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 78), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 26)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 81), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 81), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 27)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 84), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 84), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 28)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 87), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 87), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 29)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            }
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/32)*96) + (c.outer.h.outer.fused % 32))*2) + w.outer)*30) + w.inner), 1920, 3)] = conv.global[ramp((((w.outer*30) + w.inner)*3), 1, 3)]
      }
    }
  }
}
