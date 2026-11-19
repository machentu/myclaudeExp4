produce T_strided_slice {
  parallel (ax0.ax1.fused, 0, 128) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_strided_slice[(((ax0.ax1.fused*120) + (ax2*15)) + ax3)] = placeholder[(((ax0.ax1.fused*120) + (ax2*15)) + ax3)]
      }
    }
  }
}
