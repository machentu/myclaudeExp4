produce T_clip {
  for (i2, 0, 16) {
    for (i3, 0, 30) {
      T_clip[((i2*30) + i3)] = int8(max(min(placeholder[((i2*30) + i3)], (int64)127), (int64)-128))
    }
  }
}
