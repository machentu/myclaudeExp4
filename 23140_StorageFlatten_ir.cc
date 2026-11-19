// attr [DilatedInput] storage_scope = "global"
allocate DilatedInput[int8 * 1 * 64 * 63 * 119]
produce DilatedInput {
  for (i1, 0, 64) {
    for (i2, 0, 63) {
      for (i3, 0, 119) {
        DilatedInput[((((i1*63) + i2)*119) + i3)] = tvm_if_then_else((((i2 % 2) == 0) && ((i3 % 2) == 0)), placeholder[((((i1*32) + (i2/2))*60) + (i3/2))], (int8)0)
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
        PaddedInput[((((i1*65) + i2)*121) + i3)] = tvm_if_then_else(((((i2 >= 1) && (i2 < 64)) && (i3 >= 1)) && (i3 < 120)), DilatedInput[(((((i1*63) + i2)*119) + i3) + -120)], (int8)0)
      }
    }
  }
}
produce compute {
  for (c, 0, 64) {
    for (h, 0, 64) {
      for (w, 0, 120) {
        compute[((((c*64) + h)*120) + w)] = (int64)0
        for (dc, 0, 64) {
          for (dh, 0, 2) {
            for (dw, 0, 2) {
              compute[((((c*64) + h)*120) + w)] = (compute[((((c*64) + h)*120) + w)] + (int64(PaddedInput[(((((((c/64)*64) + dc)*65) + (h + dh))*121) + (w + dw))])*int64(placeholder[((((((((((c/64)*64) + dc)*64) + (c % 64))*2) - dh)*2) - dw) + 3)])))
            }
          }
        }
      }
    }
  }
}
