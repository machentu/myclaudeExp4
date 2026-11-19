produce T_sigmoid {
  parallel (ax0.ax1.fused, 0, 17) {
    for (ax3, 0, 2520) {
      T_sigmoid[((ax0.ax1.fused*2520) + ax3)] = sigmoid(placeholder[((ax0.ax1.fused*2520) + ax3)])
    }
  }
}
