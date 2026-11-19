produce T_clip {
  parallel (i0.i1.fused, 0, 16) {
    for (i2, 0, 128) {
      for (i3, 0, 240) {
        T_clip[((((i0.i1.fused*128) + i2)*240) + i3)] = int8(max(min(placeholder[((((i0.i1.fused*128) + i2)*240) + i3)], (int64)127), (int64)-128))
      }
    }
  }
}
