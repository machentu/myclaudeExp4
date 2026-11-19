produce T_cast {
  parallel (ax0.ax1.fused, 0, 17) {
    for (ax2, 0, 2) {
      for (ax3, 0, 2520) {
        T_cast[((((ax0.ax1.fused*2) + ax2)*2520) + ax3)] = float16(placeholder[((((ax0.ax1.fused*2) + ax2)*2520) + ax3)])
      }
    }
  }
}
