produce T_reshape {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 120) {
      T_reshape[((ax0.ax1.fused*120) + ax2)] = placeholder[((((((ax0.ax1.fused + (ax2/120)) % 32)*8) + ((ax2/15) % 8))*15) + (ax2 % 15))]
    }
  }
}
