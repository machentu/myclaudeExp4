produce T_multiply {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 480) {
      T_multiply[((ax0.ax1.fused*480) + ax2)] = (int32(placeholder[((ax0.ax1.fused*480) + ax2)])*int32(placeholder[0]))
    }
  }
}
