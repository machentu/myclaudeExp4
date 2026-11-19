produce T_relu {
  parallel (ax0.ax1.fused, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_relu[((((ax0.ax1.fused*16) + ax2)*8) + ax3)] = max(placeholder[((((ax0.ax1.fused*16) + ax2)*8) + ax3)], (int8)0)
      }
    }
  }
}
