produce global_sum_pool {
  parallel (ax0.ax1.fused, 0, 512) {
    global_sum_pool[ax0.ax1.fused] = 0
    for (rv1, 0, 8) {
      for (rv2, 0, 4) {
        global_sum_pool[ax0.ax1.fused] = (global_sum_pool[ax0.ax1.fused] + int32(placeholder[(((ax0.ax1.fused*32) + (rv1*4)) + rv2)]))
      }
    }
  }
}
