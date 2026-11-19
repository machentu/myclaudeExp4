produce tensor {
  parallel (ax0.ax1.fused, 0, 384) {
    for (ax2, 0, 8) {
      for (ax3, 0, 4) {
        tensor[((((ax0.ax1.fused*8) + ax2)*4) + ax3)] = 0
        for (rv, 0, 2) {
          for (rv, 0, 2) {
            tensor[((((ax0.ax1.fused*8) + ax2)*4) + ax3)] = (tensor[((((ax0.ax1.fused*8) + ax2)*4) + ax3)] + int32(placeholder[((((((((ax0.ax1.fused*8) + ax2)*2) + rv)*4) + ax3)*2) + rv)]))
          }
        }
      }
    }
  }
}
