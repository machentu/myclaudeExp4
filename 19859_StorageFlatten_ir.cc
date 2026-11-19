produce T_reshape {
  parallel (ax0.ax1.fused, 0, 4) {
    for (ax2, 0, 2520) {
      T_reshape[((ax0.ax1.fused*2520) + ax2)] = placeholder[((((ax0.ax1.fused + (ax2/2520)) % 4)*2520) + (ax2 % 2520))]
    }
  }
}
