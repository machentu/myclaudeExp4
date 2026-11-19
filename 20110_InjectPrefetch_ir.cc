// attr [compute(T_clip, 0x470df6f0)] realize_scope = ""
realize T_clip([0, 1], [0, 65], [0, 1920]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 65) {
      for (i2, 0, 1920) {
        T_clip((i0.i1.fused/65), (i0.i1.fused % 65), i2) =int8(max(min(placeholder((i0.i1.fused/65), (i0.i1.fused % 65), i2), 127), -128))
      }
    }
  }
}
