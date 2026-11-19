produce T_clip {
  parallel (i0.i1.fused, 0, 8) {
    T_clip[i0.i1.fused] = placeholder[i0.i1.fused]
  }
}
