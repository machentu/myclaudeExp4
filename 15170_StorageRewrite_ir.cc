produce T_sigmoid {
  parallel (ax0.ax1.fused, 0, 64) {
    T_sigmoid[ax0.ax1.fused] = sigmoid(placeholder[ax0.ax1.fused])
  }
}
