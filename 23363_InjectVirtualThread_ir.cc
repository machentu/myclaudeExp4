produce T_multiply {
  parallel (ax0.ax1.fused, 0, 51) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_multiply[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)] = (int64(placeholder[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
