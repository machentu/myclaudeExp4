produce T_clip {
  for (i2, 0, 32) {
    for (i3, 0, 60) {
      T_clip[((i2*60) + i3)] = int8(max(min(placeholder[((i2*60) + i3)], (int64)127), (int64)-128))
    }
  }
}
