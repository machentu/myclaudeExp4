// attr [compute(T_clip, 0x2e7f0680)] realize_scope = ""
realize T_clip([0, 1], [0, 6], [0, 1], [0, 1]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 6) {
      T_clip((i0.i1.fused/6), (i0.i1.fused % 6), 0, 0) =int32(max(min(placeholder((i0.i1.fused/6), (i0.i1.fused % 6), 0, 0), (int64)8388607), (int64)-8388608))
    }
  }
}
