// attr [compute(T_clip, 0x370012e0)] realize_scope = ""
realize T_clip([0, 1], [0, 144], [0, 120]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 144) {
      for (i2, 0, 120) {
        T_clip((i0.i1.fused/144), (i0.i1.fused % 144), i2) =int8(max(min(placeholder((i0.i1.fused/144), (i0.i1.fused % 144), i2), 127), -128))
      }
    }
  }
}
