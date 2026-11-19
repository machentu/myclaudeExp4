produce tensor {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        tensor[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] = 0
        for (rv, 0, 2) {
          for (rv, 0, 2) {
            tensor[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] = (tensor[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] + int32(placeholder[(((((ax0.ax1.fused*512) + (ax2*32)) + (rv*16)) + (ax3*2)) + rv)]))
          }
        }
      }
    }
  }
}
