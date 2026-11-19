produce T_round {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 64) {
      for (ax3, 0, 32) {
        T_round[((((ax0.ax1.fused*64) + ax2)*32) + ax3)] = round(placeholder[((((ax0.ax1.fused*64) + ax2)*32) + ax3)])
      }
    }
  }
}
