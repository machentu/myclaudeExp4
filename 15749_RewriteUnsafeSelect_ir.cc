produce T_multiply {
  parallel (ax0.ax1.fused, 0, 128) {
    T_multiply[ax0.ax1.fused] = (placeholder[ax0.ax1.fused]*placeholder[0])
  }
}
