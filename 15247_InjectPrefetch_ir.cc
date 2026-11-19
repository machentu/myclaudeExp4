// attr [compute(T_clip, 0x20ecfb60)] realize_scope = ""
realize T_clip([0, 1], [0, 64], [0, 1], [0, 1]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 64) {
      T_clip((i0.i1.fused/64), (i0.i1.fused % 64), 0, 0) =int32(max(min(placeholder((i0.i1.fused/64), (i0.i1.fused % 64), 0, 0), (int64)8388607), (int64)-8388608))
    }
  }
}
