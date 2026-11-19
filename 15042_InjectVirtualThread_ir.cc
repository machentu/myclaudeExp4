produce T_multiply {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 64) {
      for (ax3, 0, 32) {
        T_multiply[(((ax0.ax1.fused*2048) + (ax2*32)) + ax3)] = (placeholder[(((ax0.ax1.fused*2048) + (ax2*32)) + ax3)]*placeholder[0])
      }
    }
  }
}
