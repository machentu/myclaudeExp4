produce T_multiply {
  parallel (ax0.ax1.fused, 0, 4) {
    for (ax3, 0, 2520) {
      T_multiply[((ax0.ax1.fused*2520) + ax3)] = (int64(placeholder[((ax0.ax1.fused*2520) + ax3)])*int64(placeholder[0]))
    }
  }
}
