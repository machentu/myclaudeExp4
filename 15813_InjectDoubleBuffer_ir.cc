produce T_clip {
  parallel (i0.i1.fused, 0, 128) {
    T_clip[i0.i1.fused] = int32(max(min(placeholder[i0.i1.fused], (int64)8388607), (int64)-8388608))
  }
}
