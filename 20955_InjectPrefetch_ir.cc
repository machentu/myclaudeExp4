// attr [compute(T_clip, 0x344beac0)] realize_scope = ""
realize T_clip([0, 1], [0, 16], [0, 128], [0, 240]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 16) {
      for (i2, 0, 128) {
        for (i3, 0, 240) {
          T_clip((i0.i1.fused/16), (i0.i1.fused % 16), i2, i3) =int8(max(min(placeholder((i0.i1.fused/16), (i0.i1.fused % 16), i2, i3), (int64)127), (int64)-128))
        }
      }
    }
  }
}
