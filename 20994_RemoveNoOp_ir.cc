produce T_multiply {
  parallel (ax0.ax1.fused, 0, 16) {
    for (ax2, 0, 128) {
      for (ax3, 0, 240) {
        T_multiply[((((ax0.ax1.fused*128) + ax2)*240) + ax3)] = (int64(placeholder[((((ax0.ax1.fused*128) + ax2)*240) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
