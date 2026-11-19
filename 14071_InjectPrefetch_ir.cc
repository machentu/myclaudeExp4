// attr [compute(T_clip, 0x2eb345a0)] realize_scope = ""
realize T_clip([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 384) {
      for (i2, 0, 16) {
        for (i3, 0, 8) {
          T_clip((i0.i1.fused/384), (i0.i1.fused % 384), i2, i3) =max(min(placeholder((i0.i1.fused/384), (i0.i1.fused % 384), i2, i3), 127.000000h), -128.000000h)
        }
      }
    }
  }
}
