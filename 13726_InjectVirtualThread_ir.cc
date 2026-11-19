produce T_cast {
  parallel (ax0.ax1.fused, 0, 512) {
    for (ax2, 0, 8) {
      for (ax3, 0, 4) {
        T_cast[(((ax0.ax1.fused*32) + (ax2*4)) + ax3)] = int8(placeholder[(((ax0.ax1.fused*32) + (ax2*4)) + ax3)])
      }
    }
  }
}
