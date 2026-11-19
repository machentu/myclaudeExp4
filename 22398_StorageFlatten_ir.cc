produce T_sigmoid {
  parallel (ax0.ax1.fused, 0, 80) {
    for (ax2, 0, 2520) {
      T_sigmoid[((ax0.ax1.fused*2520) + ax2)] = sigmoid(placeholder[((ax0.ax1.fused*2520) + ax2)])
    }
  }
}
