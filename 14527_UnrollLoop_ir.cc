produce T_clip {
  parallel (i0.i1.fused, 0, 256) {
    for (i2, 0, 32) {
      for (i3, 0, 16) {
        T_clip[(((i0.i1.fused*512) + (i2*16)) + i3)] = int8(max(min(placeholder[(((i0.i1.fused*512) + (i2*16)) + i3)], (int64)127), (int64)-128))
      }
    }
  }
}
