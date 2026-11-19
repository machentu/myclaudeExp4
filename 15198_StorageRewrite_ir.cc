produce T_cast {
  parallel (ax0.ax1.fused, 0, 64) {
    T_cast[ax0.ax1.fused] = float16(placeholder[ax0.ax1.fused])
  }
}
