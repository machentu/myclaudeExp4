// attr [compute(T_clip, 0x421ab3a0)] realize_scope = ""
realize T_clip([0, 1], [0, 51], [0, 8], [0, 15]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 51) {
      for (i2, 0, 8) {
        for (i3, 0, 15) {
          T_clip((i0.i1.fused/51), (i0.i1.fused % 51), i2, i3) =max(min(placeholder((i0.i1.fused/51), (i0.i1.fused % 51), i2, i3), (int8)127), (int8)-128)
        }
      }
    }
  }
}
