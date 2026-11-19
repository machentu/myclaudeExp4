produce T_clip {
  parallel (i0.i1.fused, 0, 128) {
    for (i2, 0, 8) {
      for (i3, 0, 4) {
        T_clip[((((i0.i1.fused*8) + i2)*4) + i3)] = int32(max(min(placeholder[((((i0.i1.fused*8) + i2)*4) + i3)], (int64)8388607), (int64)-8388608))
      }
    }
  }
}
