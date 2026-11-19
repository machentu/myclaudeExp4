// attr [compute(T_clip, 0x2eb20eb0)] realize_scope = ""
realize T_clip([0, 1], [0, 64], [0, 32], [0, 16]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 64) {
      for (i2, 0, 32) {
        for (i3, 0, 16) {
          T_clip((i0.i1.fused/64), (i0.i1.fused % 64), i2, i3) =int32(max(min(placeholder((i0.i1.fused/64), (i0.i1.fused % 64), i2, i3), (int64)8388607), (int64)-8388608))
        }
      }
    }
  }
}
