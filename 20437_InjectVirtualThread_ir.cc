produce T_multiply {
  parallel (ax0.ax1.fused, 0, 192) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_multiply[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)] = (int64(placeholder[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)])*int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
