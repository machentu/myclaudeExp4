// attr [compute(T_clip, 0x37c248a0)] realize_scope = ""
realize T_clip([0, 1], [0, 32], [0, 2520]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 32) {
      for (i2, 0, 2520) {
        T_clip((i0.i1.fused/32), (i0.i1.fused % 32), i2) =int8(max(min(placeholder((i0.i1.fused/32), (i0.i1.fused % 32), i2), (int64)127), (int64)-128))
      }
    }
  }
}
