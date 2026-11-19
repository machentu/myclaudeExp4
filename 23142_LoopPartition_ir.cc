// attr [DilatedInput] storage_scope = "global"
allocate DilatedInput[int8 * 1 * 64 * 63 * 119]
produce DilatedInput {
  for (i1, 0, 64) {
    for (i2, 0, 63) {
      for (i3, 0, 119) {
        DilatedInput[(((i1*7497) + (i2*119)) + i3)] = tvm_if_then_else((((i2 % 2) == 0) && ((i3 % 2) == 0)), placeholder[(((i1*1920) + ((i2/2)*60)) + (i3/2))], (int8)0)
      }
    }
  }
}
// attr [PaddedInput] storage_scope = "global"
allocate PaddedInput[int8 * 1 * 64 * 65 * 121]
produce PaddedInput {
  for (i1, 0, 64) {
    for (i2, 0, 65) {
      for (i3, 0, 121) {
        PaddedInput[(((i1*7865) + (i2*121)) + i3)] = tvm_if_then_else(((((1 <= i2) && (i2 < 64)) && (1 <= i3)) && (i3 < 120)), DilatedInput[((((i1*7497) + (i2*119)) + i3) - 120)], (int8)0)
      }
    }
  }
}
produce compute {
  for (c, 0, 64) {
    for (h, 0, 64) {
      for (w, 0, 120) {
        compute[(((c*7680) + (h*120)) + w)] = (int64)0
        for (dc, 0, 64) {
          for (dh, 0, 2) {
            for (dw, 0, 2) {
              compute[(((c*7680) + (h*120)) + w)] = (compute[(((c*7680) + (h*120)) + w)] + (int64(PaddedInput[(((((dc*7865) + (h*121)) + (dh*121)) + w) + dw)])*int64(placeholder[(((((dc*256) + (c*4)) + 3) - dw) - (dh*2))])))
            }
          }
        }
      }
    }
  }
}
