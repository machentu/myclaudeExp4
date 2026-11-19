produce T_cast {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax3, 0, 2520) {
      T_cast[((ax0.ax1.fused*2520) + ax3)] = int8(placeholder[((ax0.ax1.fused*2520) + ax3)])
    }
  }
}
