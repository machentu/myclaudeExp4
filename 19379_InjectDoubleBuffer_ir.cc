produce T_clip {
  parallel (i0.i1.fused, 0, 64) {
    for (i3, 0, 2520) {
      T_clip[((i0.i1.fused*2520) + i3)] = max(min(placeholder[((i0.i1.fused*2520) + i3)], 127.000000h), -128.000000h)
    }
  }
}
