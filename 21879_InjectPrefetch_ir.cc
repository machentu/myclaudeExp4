// attr [compute(T_clip, 0x3c2be480)] realize_scope = ""
realize T_clip([0, 1], [0, 80], [0, 16], [0, 30]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 80) {
      for (i2, 0, 16) {
        for (i3, 0, 30) {
          T_clip((i0.i1.fused/80), (i0.i1.fused % 80), i2, i3) =int8(max(min(placeholder((i0.i1.fused/80), (i0.i1.fused % 80), i2, i3), (int64)127), (int64)-128))
        }
      }
    }
  }
}
