// attr [compute(T_clip, 0x2e2386f0)] realize_scope = ""
realize T_clip([0, 1], [0, 4], [0, 1], [0, 1]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 4) {
      T_clip((i0.i1.fused/4), (i0.i1.fused % 4), 0, 0) =max(min(placeholder((i0.i1.fused/4), (i0.i1.fused % 4), 0, 0), (int8)127), (int8)-128)
    }
  }
}
