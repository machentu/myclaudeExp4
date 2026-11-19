// attr [compute(T_clip, 0x142c3310)] realize_scope = ""
realize T_clip([0, 1], [0, 512], [0, 8], [0, 4]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 512) {
      for (i2, 0, 8) {
        for (i3, 0, 4) {
          T_clip((i0.i1.fused/512), (i0.i1.fused % 512), i2, i3) =int32(max(min(placeholder((i0.i1.fused/512), (i0.i1.fused % 512), i2, i3), (int64)8388607), (int64)-8388608))
        }
      }
    }
  }
}
