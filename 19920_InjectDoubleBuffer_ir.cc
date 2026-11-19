produce T_clip {
  parallel (i0.i1.fused, 0, 4) {
    for (i3, 0, 2520) {
      T_clip[((i0.i1.fused*2520) + i3)] = int32(max(min(placeholder[((i0.i1.fused*2520) + i3)], (int64)8388607), (int64)-8388608))
    }
  }
}
