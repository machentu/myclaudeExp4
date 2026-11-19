produce T_relu {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_relu[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)] = max(placeholder[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)], (int8)0)
      }
    }
  }
}
