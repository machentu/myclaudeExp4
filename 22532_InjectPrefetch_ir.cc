// attr [compute(T_clip, 0x43bab1e0)] realize_scope = ""
realize T_clip([0, 1], [0, 1], [0, 16], [0, 30]) {
  produce T_clip {
    for (i2, 0, 16) {
      for (i3, 0, 30) {
        T_clip(0, 0, i2, i3) =int32(max(min(placeholder(0, 0, i2, i3), (int64)8388607), (int64)-8388608))
      }
    }
  }
}
