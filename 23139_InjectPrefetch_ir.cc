// attr [compute(DilatedInput, 0x38f2bd10)] realize_scope = ""
realize DilatedInput([0, 1], [0, 64], [0, 63], [0, 119]) {
  produce DilatedInput {
    for (i1, 0, 64) {
      for (i2, 0, 63) {
        for (i3, 0, 119) {
          DilatedInput(0, i1, i2, i3) =tvm_if_then_else((((i2 % 2) == 0) && ((i3 % 2) == 0)), placeholder(0, i1, (i2/2), (i3/2)), (int8)0)
        }
      }
    }
  }
  // attr [compute(PaddedInput, 0x38f2f380)] realize_scope = ""
  realize PaddedInput([0, 1], [0, 64], [0, 65], [0, 121]) {
    produce PaddedInput {
      for (i1, 0, 64) {
        for (i2, 0, 65) {
          for (i3, 0, 121) {
            PaddedInput(0, i1, i2, i3) =tvm_if_then_else(((((i2 >= 1) && (i2 < 64)) && (i3 >= 1)) && (i3 < 120)), DilatedInput(0, i1, (i2 - 1), (i3 - 1)), (int8)0)
          }
        }
      }
    }
    // attr [compute(compute, 0x381708d0)] realize_scope = ""
    realize compute([0, 1], [0, 64], [0, 64], [0, 120]) {
      produce compute {
        for (c, 0, 64) {
          for (h, 0, 64) {
            for (w, 0, 120) {
              compute(0, c, h, w) =(int64)0
              for (dc, 0, 64) {
                for (dh, 0, 2) {
                  for (dw, 0, 2) {
                    compute(0, c, h, w) =(compute(0, c, h, w) + (int64(PaddedInput(0, (((c/64)*64) + dc), (h + dh), (w + dw)))*int64(placeholder((((c/64)*64) + dc), (c % 64), (1 - dh), (1 - dw)))))
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
