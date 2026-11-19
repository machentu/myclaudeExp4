produce T_clip {
  for (i2, 0, 32) {
    for (i3, 0, 60) {
      T_clip[((i2*60) + i3)] = int32(max(min(placeholder[((i2*60) + i3)], (int64)8388607), (int64)-8388608))
    }
  }
}
