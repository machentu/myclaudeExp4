produce T_multiply {
  parallel (ax0.ax1.fused, 0, 6) {
    T_multiply[ax0.ax1.fused] = (int64(placeholder[ax0.ax1.fused])*int64(placeholder[(ax0.ax1.fused % 6)]))
  }
}
