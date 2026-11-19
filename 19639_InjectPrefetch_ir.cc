// attr [compute(T_clip, 0x3577e660)] realize_scope = ""
realize T_clip([0, 1], [0, 64], [0, 32], [0, 60]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 64) {
      for (i2, 0, 32) {
        for (i3, 0, 60) {
          T_clip((i0.i1.fused/64), (i0.i1.fused % 64), i2, i3) =max(min(placeholder((i0.i1.fused/64), (i0.i1.fused % 64), i2, i3), (int8)127), (int8)-128)
        }
      }
    }
  }
}
