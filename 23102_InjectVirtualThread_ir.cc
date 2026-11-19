produce T_clip {
  parallel (i0.i1.fused, 0, 64) {
    for (i2, 0, 64) {
      for (i3, 0, 120) {
        T_clip[(((i0.i1.fused*7680) + (i2*120)) + i3)] = int32(max(min(placeholder[(((i0.i1.fused*7680) + (i2*120)) + i3)], (int64)8388607), (int64)-8388608))
      }
    }
  }
}
