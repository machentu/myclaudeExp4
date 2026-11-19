// attr [compute(T_clip, 0x47f7a6b0)] realize_scope = ""
realize T_clip([0, 1], [0, 1], [0, 32], [0, 60]) {
  produce T_clip {
    for (i2, 0, 32) {
      for (i3, 0, 60) {
        T_clip(0, 0, i2, i3) =int32(max(min(placeholder(0, 0, i2, i3), (int64)8388607), (int64)-8388608))
      }
    }
  }
}
