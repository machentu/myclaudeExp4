produce T_clip {
  parallel (i0.i1.fused, 0, 384) {
    for (i2, 0, 16) {
      for (i3, 0, 8) {
        T_clip[((((i0.i1.fused*16) + i2)*8) + i3)] = placeholder[((((i0.i1.fused*16) + i2)*8) + i3)]
      }
    }
  }
}
