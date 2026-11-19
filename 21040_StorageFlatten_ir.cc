produce T_strided_slice {
  parallel (ax0.ax1.fused, 0, 16) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_strided_slice[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] = placeholder[((((((((ax0.ax1.fused/16)*32) + (ax0.ax1.fused % 16))*64) + ax2)*120) + ax3) + 122880)]
      }
    }
  }
}
