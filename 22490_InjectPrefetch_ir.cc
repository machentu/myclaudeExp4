// attr [compute(T_clip, 0x4896e240)] realize_scope = ""
realize T_clip([0, 1], [0, 1], [0, 16], [0, 30]) {
  produce T_clip {
    for (i2, 0, 16) {
      for (i3, 0, 30) {
        T_clip(0, 0, i2, i3) =int8(max(min(placeholder(0, 0, i2, i3), (int64)127), (int64)-128))
      }
    }
  }
}
