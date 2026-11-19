// attr [compute(global_sum_pool, 0x1c17fbc0)] realize_scope = ""
realize global_sum_pool([0, 1], [0, 512], [0, 1], [0, 1]) {
  produce global_sum_pool {
    parallel (ax0.ax1.fused, 0, 512) {
      global_sum_pool((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0, 0) =0
      for (rv1, 0, 8) {
        for (rv2, 0, 4) {
          global_sum_pool((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0, 0) =(global_sum_pool((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0, 0) + int32(placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ((0*8) + rv1), ((0*4) + rv2))))
        }
      }
    }
  }
}
