produce T_relu {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_relu[(((ax0.ax1.fused*512) + (ax2*16)) + ax3)] = max(placeholder[(((ax0.ax1.fused*512) + (ax2*16)) + ax3)], (int8)0)
      }
    }
  }
}
