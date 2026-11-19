produce T_strided_slice {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_strided_slice[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)] = placeholder[((((ax0.ax1.fused*480) + (ax2*30)) + ax3) + 30720)]
      }
    }
  }
}
