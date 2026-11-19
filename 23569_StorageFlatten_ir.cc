produce T_clip {
  parallel (i0.i1.fused, 0, 51) {
    for (i2, 0, 8) {
      for (i3, 0, 15) {
        T_clip[((((i0.i1.fused*8) + i2)*15) + i3)] = max(min(placeholder[((((i0.i1.fused*8) + i2)*15) + i3)], (int8)127), (int8)-128)
      }
    }
  }
}
