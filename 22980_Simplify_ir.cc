produce T_cast {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_cast[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] = float32(placeholder[((((ax0.ax1.fused*64) + ax2)*120) + ax3)])
      }
    }
  }
}
