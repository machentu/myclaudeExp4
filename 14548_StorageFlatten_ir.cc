produce T_multiply {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_multiply[((((ax0.ax1.fused*32) + ax2)*16) + ax3)] = (int64(placeholder[((((ax0.ax1.fused*32) + ax2)*16) + ax3)])*int64(placeholder[(ax0.ax1.fused % 256)]))
      }
    }
  }
}
