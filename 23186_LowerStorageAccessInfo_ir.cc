produce T_clip {
  parallel (i0.i1.fused, 0, 51) {
    for (i2, 0, 32) {
      for (i3, 0, 60) {
        T_clip[((((i0.i1.fused*32) + i2)*60) + i3)] = int32(max(min(placeholder[((((i0.i1.fused*32) + i2)*60) + i3)], (int64)8388607), (int64)-8388608))
      }
    }
  }
}
