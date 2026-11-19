produce T_clip {
  parallel (i0.i1.fused, 0, 512) {
    T_clip[i0.i1.fused] = int8(max(min(placeholder[i0.i1.fused], (int64)127), (int64)-128))
  }
}
