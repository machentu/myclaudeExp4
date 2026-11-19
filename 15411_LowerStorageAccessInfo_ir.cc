produce tensor {
  parallel (ax0.ax1.fused, 0, 64) {
    tensor[ax0.ax1.fused] = 0
    for (rv, 0, 32) {
      for (rv, 0, 16) {
        tensor[ax0.ax1.fused] = (tensor[ax0.ax1.fused] + int32(placeholder[((((ax0.ax1.fused*32) + rv)*16) + rv)]))
      }
    }
  }
}
