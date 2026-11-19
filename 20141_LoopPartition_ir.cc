produce T_multiply {
  parallel (ax0.ax1.fused, 0, 65) {
    for (ax2, 0, 1920) {
      T_multiply[((ax0.ax1.fused*1920) + ax2)] = (int32(placeholder[((ax0.ax1.fused*1920) + ax2)])*int32(placeholder[0]))
    }
  }
}
