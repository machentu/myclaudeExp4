produce T_clip {
  parallel (i0.i1.fused, 0, 32) {
    for (i2, 0, 64) {
      for (i3, 0, 120) {
        T_clip[((((i0.i1.fused*64) + i2)*120) + i3)] = max(min(placeholder[((((i0.i1.fused*64) + i2)*120) + i3)], (int8)127), (int8)-128)
      }
    }
  }
}
