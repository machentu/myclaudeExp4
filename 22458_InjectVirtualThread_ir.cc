produce T_multiply {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 2520) {
      T_multiply[((ax0.ax1.fused*2520) + ax2)] = (placeholder[((ax0.ax1.fused*2520) + ax2)]*placeholder[0])
    }
  }
}
