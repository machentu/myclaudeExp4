produce T_multiply {
  parallel (ax0.ax1.fused, 0, 384) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_multiply[(((ax0.ax1.fused*120) + (ax2*15)) + ax3)] = (int64(placeholder[(((ax0.ax1.fused*120) + (ax2*15)) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
