produce T_relu {
  parallel (ax0.ax1.fused, 0, 8) {
    T_relu[ax0.ax1.fused] = max(placeholder[ax0.ax1.fused], (int8)0)
  }
}
