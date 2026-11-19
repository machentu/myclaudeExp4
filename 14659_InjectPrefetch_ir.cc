// attr [compute(T_clip, 0x22903450)] realize_scope = ""
realize T_clip([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 256) {
      for (i2, 0, 32) {
        for (i3, 0, 16) {
          T_clip((i0.i1.fused/256), (i0.i1.fused % 256), i2, i3) =max(min(placeholder((i0.i1.fused/256), (i0.i1.fused % 256), i2, i3), 127.000000h), -128.000000h)
        }
      }
    }
  }
}
