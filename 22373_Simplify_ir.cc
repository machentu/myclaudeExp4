produce T_clip {
  parallel (i0.i1.fused, 0, 65) {
    for (i2, 0, 480) {
      T_clip[((i0.i1.fused*480) + i2)] = int8(max(min(placeholder[((i0.i1.fused*480) + i2)], 127), -128))
    }
  }
}
