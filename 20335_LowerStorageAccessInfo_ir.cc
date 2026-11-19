produce T_relu {
  parallel (ax0.ax1.fused, 0, 128) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_relu[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = max(placeholder[((((ax0.ax1.fused*8) + ax2)*15) + ax3)], (int8)0)
      }
    }
  }
}
