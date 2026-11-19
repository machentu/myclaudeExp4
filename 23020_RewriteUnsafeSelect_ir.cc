produce T_reshape {
  parallel (ax0.ax1.fused, 0, 17) {
    for (ax2, 0, 3) {
      for (ax3, 0, 2520) {
        T_reshape[((((ax0.ax1.fused*3) + ax2)*2520) + ax3)] = placeholder[((((ax0.ax1.fused*3) + ax2)*2520) + ax3)]
      }
    }
  }
}
