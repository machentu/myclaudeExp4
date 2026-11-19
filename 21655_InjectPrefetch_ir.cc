// attr [compute(T_clip, 0x372d67e0)] realize_scope = ""
realize T_clip([0, 1], [0, 80], [0, 32], [0, 60]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 80) {
      for (i2, 0, 32) {
        for (i3, 0, 60) {
          T_clip((i0.i1.fused/80), (i0.i1.fused % 80), i2, i3) =int32(max(min(placeholder((i0.i1.fused/80), (i0.i1.fused % 80), i2, i3), (int64)8388607), (int64)-8388608))
        }
      }
    }
  }
}
