produce T_clip {
  parallel (i0.i1.fused, 0, 51) {
    for (i2, 0, 2520) {
      T_clip[((i0.i1.fused*2520) + i2)] = int8(max(min(placeholder[((i0.i1.fused*2520) + i2)], (int64)127), (int64)-128))
    }
  }
}
