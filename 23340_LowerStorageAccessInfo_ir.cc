produce T_clip {
  parallel (i0.i1.fused, 0, 51) {
    for (i2, 0, 16) {
      for (i3, 0, 30) {
        T_clip[((((i0.i1.fused*16) + i2)*30) + i3)] = int8(max(min(placeholder[((((i0.i1.fused*16) + i2)*30) + i3)], (int64)127), (int64)-128))
      }
    }
  }
}
