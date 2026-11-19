produce T_multiply {
  parallel (ax0.ax1.fused, 0, 128) {
    for (ax2, 0, 8) {
      for (ax3, 0, 4) {
        T_multiply[(((ax0.ax1.fused*32) + (ax2*4)) + ax3)] = (int64(placeholder[(((ax0.ax1.fused*32) + (ax2*4)) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
