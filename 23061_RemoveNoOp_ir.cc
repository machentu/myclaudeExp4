produce T_multiply {
  parallel (ax0.ax1.fused, 0, 51) {
    for (ax2, 0, 2520) {
      T_multiply[((ax0.ax1.fused*2520) + ax2)] = (int64(placeholder[((ax0.ax1.fused*2520) + ax2)])*int64(placeholder[0]))
    }
  }
}
