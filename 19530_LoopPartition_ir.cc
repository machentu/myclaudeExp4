produce T_reshape {
  parallel (ax0.ax1.fused, 0, 144) {
    for (ax2, 0, 1920) {
      T_reshape[((ax0.ax1.fused*1920) + ax2)] = placeholder[((ax0.ax1.fused*1920) + ax2)]
    }
  }
}
