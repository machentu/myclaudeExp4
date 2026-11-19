produce T_cast {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax0.ax1.fused*32) + ax2)*16) + ax3)] = int8(placeholder[((((ax0.ax1.fused*32) + ax2)*16) + ax3)])
      }
    }
  }
}
