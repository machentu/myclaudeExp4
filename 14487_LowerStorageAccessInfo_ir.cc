produce tensor {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        tensor[((((ax0.ax1.fused*16) + ax2)*8) + ax3)] = 0
        for (rv, 0, 2) {
          for (rv, 0, 2) {
            tensor[((((ax0.ax1.fused*16) + ax2)*8) + ax3)] = (tensor[((((ax0.ax1.fused*16) + ax2)*8) + ax3)] + int32(placeholder[((((((((ax0.ax1.fused*16) + ax2)*2) + rv)*8) + ax3)*2) + rv)]))
          }
        }
      }
    }
  }
}
