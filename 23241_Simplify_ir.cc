produce T_relu {
  parallel (ax0.ax1.fused, 0, 51) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_relu[((((ax0.ax1.fused*32) + ax2)*60) + ax3)] = max(placeholder[((((ax0.ax1.fused*32) + ax2)*60) + ax3)], (int8)0)
      }
    }
  }
}
