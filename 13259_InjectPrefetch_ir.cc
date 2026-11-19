// attr [compute(T_clip, 0x2a08b8d0)] realize_scope = ""
realize T_clip([0, 1], [0, 512], [0, 1], [0, 1]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 512) {
      T_clip((i0.i1.fused/512), (i0.i1.fused % 512), 0, 0) =int8(max(min(placeholder((i0.i1.fused/512), (i0.i1.fused % 512), 0, 0), (int64)127), (int64)-128))
    }
  }
}
