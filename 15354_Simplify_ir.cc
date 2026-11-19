produce T_multiply {
  parallel (ax0.ax1.fused, 0, 4) {
    T_multiply[ax0.ax1.fused] = (int64(placeholder[ax0.ax1.fused])*int64(placeholder[ax0.ax1.fused]))
  }
}
