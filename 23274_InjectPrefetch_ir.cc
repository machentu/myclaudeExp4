// attr [compute(T_clip, 0x4819ce60)] realize_scope = ""
realize T_clip([0, 1], [0, 51], [0, 480]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 51) {
      for (i2, 0, 480) {
        T_clip((i0.i1.fused/51), (i0.i1.fused % 51), i2) =int8(max(min(placeholder((i0.i1.fused/51), (i0.i1.fused % 51), i2), 127), -128))
      }
    }
  }
}
