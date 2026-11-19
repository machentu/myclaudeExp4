produce T_reshape {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 480) {
      T_reshape[((ax0.ax1.fused*480) + ax2)] = placeholder[((ax0.ax1.fused*480) + ax2)]
    }
  }
}
