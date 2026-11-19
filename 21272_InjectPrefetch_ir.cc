// attr [compute(T_clip, 0x4915e280)] realize_scope = ""
realize T_clip([0, 1], [0, 128], [0, 32], [0, 60]) {
  produce T_clip {
    parallel (i0.i1.fused, 0, 128) {
      for (i2, 0, 32) {
        for (i3, 0, 60) {
          T_clip((i0.i1.fused/128), (i0.i1.fused % 128), i2, i3) =int8(max(min(placeholder((i0.i1.fused/128), (i0.i1.fused % 128), i2, i3), (int64)127), (int64)-128))
        }
      }
    }
  }
}
