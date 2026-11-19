produce T_clip {
  parallel (i0.i1.fused, 0, 65) {
    for (i2, 0, 1920) {
      T_clip[((i0.i1.fused*1920) + i2)] = int8(max(min(placeholder[((i0.i1.fused*1920) + i2)], 127), -128))
    }
  }
}
