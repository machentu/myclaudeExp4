produce T_cast {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 64) {
      for (ax3, 0, 32) {
        T_cast[(((ax0.ax1.fused*2048) + (ax2*32)) + ax3)] = float16(placeholder[(((ax0.ax1.fused*2048) + (ax2*32)) + ax3)])
      }
    }
  }
}
