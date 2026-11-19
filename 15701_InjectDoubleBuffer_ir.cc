produce tensor {
  parallel (ax0.ax1.fused, 0, 96) {
    tensor[ax0.ax1.fused] = 0
    for (rv, 0, 16) {
      for (rv, 0, 8) {
        tensor[ax0.ax1.fused] = (tensor[ax0.ax1.fused] + int32(placeholder[(((ax0.ax1.fused*128) + (rv*8)) + rv)]))
      }
    }
  }
}
