// attr [compute(T_clip, 0x3a9133e0)] realize_scope = ""
realize T_clip([0, 1], [0, 48], [0, 64], [0, 120]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 48) {
      for (i2, 0, 64) {
        for (i3, 0, 120) {
          T_clip((i0.i1.fused/48), (i0.i1.fused % 48), i2, i3) =int8(max(min(placeholder((i0.i1.fused/48), (i0.i1.fused % 48), i2, i3), (int64)127), (int64)-128))
        }
      }
    }
  }
}
