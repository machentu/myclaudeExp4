produce T_clip {
  for (i2, 0, 8) {
    for (i3, 0, 15) {
      T_clip[((i2*15) + i3)] = int8(max(min(placeholder[((i2*15) + i3)], (int64)127), (int64)-128))
    }
  }
}
