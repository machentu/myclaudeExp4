produce T_round {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_round[(((ax0.ax1.fused*512) + (ax2*16)) + ax3)] = round(placeholder[(((ax0.ax1.fused*512) + (ax2*16)) + ax3)])
      }
    }
  }
}
