produce T_round {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax3, 0, 2520) {
      T_round[((ax0.ax1.fused*2520) + ax3)] = round(placeholder[((ax0.ax1.fused*2520) + ax3)])
    }
  }
}
