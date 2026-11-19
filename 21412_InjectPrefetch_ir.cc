// attr [compute(T_clip, 0x4dca69e0)] realize_scope = ""
realize T_clip([0, 1], [0, 32], [0, 64], [0, 120]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 32) {
      for (i2, 0, 64) {
        for (i3, 0, 120) {
          T_clip((i0.i1.fused/32), (i0.i1.fused % 32), i2, i3) =int32(max(min(placeholder((i0.i1.fused/32), (i0.i1.fused % 32), i2, i3), (int64)8388607), (int64)-8388608))
        }
      }
    }
  }
}
