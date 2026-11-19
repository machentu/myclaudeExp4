produce T_clip {
  parallel (i0.i1.fused, 0, 64) {
    for (i2, 0, 16) {
      for (i3, 0, 30) {
        T_clip[((((i0.i1.fused*16) + i2)*30) + i3)] = placeholder[((((i0.i1.fused*16) + i2)*30) + i3)]
      }
    }
  }
}
