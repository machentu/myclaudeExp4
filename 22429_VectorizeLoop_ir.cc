produce T_cast {
  parallel (ax0.ax1.fused, 0, 80) {
    for (ax2, 0, 2520) {
      T_cast[((ax0.ax1.fused*2520) + ax2)] = float16(placeholder[((ax0.ax1.fused*2520) + ax2)])
    }
  }
}
