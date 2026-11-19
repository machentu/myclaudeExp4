produce T_multiply {
  parallel (ax0.ax1.fused, 0, 96) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_multiply[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] = (int64(placeholder[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
