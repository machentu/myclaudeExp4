produce T_multiply {
  parallel (ax0.ax1.fused, 0, 48) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_multiply[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] = (int64(placeholder[((((ax0.ax1.fused*64) + ax2)*120) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
