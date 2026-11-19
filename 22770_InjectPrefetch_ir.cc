// attr [compute(T_clip, 0x4d95c400)] realize_scope = ""
realize T_clip([0, 1], [0, 1], [0, 8], [0, 15]) {
  produce T_clip {
    for (i2, 0, 8) {
      for (i3, 0, 15) {
        T_clip(0, 0, i2, i3) =int8(max(min(placeholder(0, 0, i2, i3), (int64)127), (int64)-128))
      }
    }
  }
}
