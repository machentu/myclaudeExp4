produce T_multiply {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax3, 0, 2520) {
      T_multiply[((ax0.ax1.fused*2520) + ax3)] = (placeholder[((ax0.ax1.fused*2520) + ax3)]*placeholder[0])
    }
  }
}
