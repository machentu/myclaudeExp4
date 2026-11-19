produce T_multiply {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_multiply[((((ax0.ax1.fused*32) + ax2)*60) + ax3)] = (int32(placeholder[((((ax0.ax1.fused*32) + ax2)*60) + ax3)])*int32(placeholder[(ax0.ax1.fused % 32)]))
      }
    }
  }
}
